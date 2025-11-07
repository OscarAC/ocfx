/* OCFX - Wayland Client Implementation
 * Generic Wayland window management
 */

#define _POSIX_C_SOURCE 200809L  /* For strdup */

#include "ocfx/wayland.h"
#include "ocfx/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <wayland-egl.h>
#include <EGL/egl.h>

/* Need xdg-shell protocol - apps must generate this */
#include "xdg-shell-protocol.h"

/* Forward declaration for internal EGL access */
struct wl_egl_window* ocfx_window_get_egl_window(ocfx_window_t *window);

/* Window structure (opaque to users) */
struct ocfx_window_t {
    /* Wayland core */
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_surface *surface;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;

    /* XDG shell */
    struct xdg_wm_base *xdg_wm_base;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;

    /* EGL window (for rendering) */
    struct wl_egl_window *egl_window;

    /* XKB keyboard */
    struct xkb_context *xkb_context;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;

    /* Window state */
    int32_t width;
    int32_t height;
    bool configured;
    bool should_close;
    char *title;
    char *app_id;

    /* User data */
    void *user_data;

    /* Callbacks */
    ocfx_resize_callback_t resize_callback;
    ocfx_close_callback_t close_callback;
    ocfx_focus_callback_t focus_callback;

    /* Input state (for queries) */
    double mouse_x, mouse_y;
    bool mouse_buttons[8];  /* Track button state */
    uint32_t modifiers;

    /* Input callbacks (set via ocfx/input.h) */
    void *key_callback;
    void *mouse_callback;

    /* Key repeat info (compositor handles the actual repeat) */
    int32_t repeat_rate;      /* Keys per second */
    int32_t repeat_delay;     /* Initial delay in milliseconds */
};

/* Forward declarations for listeners */
static void registry_global_handler(void *data, struct wl_registry *registry,
                                    uint32_t name, const char *interface,
                                    uint32_t version);
static void registry_global_remove_handler(void *data, struct wl_registry *registry,
                                           uint32_t name);
static void xdg_wm_base_ping_handler(void *data, struct xdg_wm_base *xdg_wm_base,
                                     uint32_t serial);
static void xdg_surface_configure_handler(void *data, struct xdg_surface *xdg_surface,
                                          uint32_t serial);
static void xdg_toplevel_configure_handler(void *data, struct xdg_toplevel *xdg_toplevel,
                                           int32_t width, int32_t height,
                                           struct wl_array *states);
static void xdg_toplevel_close_handler(void *data, struct xdg_toplevel *xdg_toplevel);
static void seat_capabilities_handler(void *data, struct wl_seat *seat, uint32_t capabilities);
static void seat_name_handler(void *data, struct wl_seat *seat, const char *name);
static void keyboard_keymap_handler(void *data, struct wl_keyboard *keyboard,
                                   uint32_t format, int32_t fd, uint32_t size);
static void keyboard_enter_handler(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface,
                                  struct wl_array *keys);
static void keyboard_leave_handler(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface);
static void keyboard_key_handler(void *data, struct wl_keyboard *keyboard,
                                uint32_t serial, uint32_t time,
                                uint32_t key, uint32_t state_key);
static void keyboard_modifiers_handler(void *data, struct wl_keyboard *keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched, uint32_t mods_locked,
                                      uint32_t group);
static void keyboard_repeat_info_handler(void *data, struct wl_keyboard *keyboard,
                                        int32_t rate, int32_t delay);
static void pointer_enter_handler(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface,
                                 wl_fixed_t surface_x, wl_fixed_t surface_y);
static void pointer_leave_handler(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface);
static void pointer_motion_handler(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y);
static void pointer_button_handler(void *data, struct wl_pointer *pointer,
                                  uint32_t serial, uint32_t time, uint32_t button,
                                  uint32_t state_button);
static void pointer_axis_handler(void *data, struct wl_pointer *pointer,
                                uint32_t time, uint32_t axis, wl_fixed_t value);
static void pointer_frame_handler(void *data, struct wl_pointer *pointer);

/* Listener structures */
static const struct wl_registry_listener registry_listener = {
    .global = registry_global_handler,
    .global_remove = registry_global_remove_handler,
};

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping_handler,
};

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure_handler,
};

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure_handler,
    .close = xdg_toplevel_close_handler,
};

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities_handler,
    .name = seat_name_handler,
};

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap_handler,
    .enter = keyboard_enter_handler,
    .leave = keyboard_leave_handler,
    .key = keyboard_key_handler,
    .modifiers = keyboard_modifiers_handler,
    .repeat_info = keyboard_repeat_info_handler,
};

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter_handler,
    .leave = pointer_leave_handler,
    .motion = pointer_motion_handler,
    .button = pointer_button_handler,
    .axis = pointer_axis_handler,
    .frame = pointer_frame_handler,
};

/* ============================================================================
 * Registry Handlers
 * ============================================================================ */

static void registry_global_handler(void *data, struct wl_registry *registry,
                                    uint32_t name, const char *interface,
                                    uint32_t version) {
    ocfx_window_t *window = data;
    (void)version;  /* Unused */

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        window->compositor = wl_registry_bind(registry, name,
                                              &wl_compositor_interface, 4);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        window->xdg_wm_base = wl_registry_bind(registry, name,
                                                &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(window->xdg_wm_base, &xdg_wm_base_listener, window);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        window->seat = wl_registry_bind(registry, name, &wl_seat_interface, 5);
        wl_seat_add_listener(window->seat, &seat_listener, window);
    }
}

static void registry_global_remove_handler(void *data, struct wl_registry *registry,
                                           uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
    /* Handle global removal if needed */
}

/* ============================================================================
 * XDG Shell Handlers
 * ============================================================================ */

static void xdg_wm_base_ping_handler(void *data, struct xdg_wm_base *xdg_wm_base,
                                     uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static void xdg_surface_configure_handler(void *data, struct xdg_surface *xdg_surface,
                                          uint32_t serial) {
    ocfx_window_t *window = data;
    xdg_surface_ack_configure(xdg_surface, serial);
    window->configured = true;
}

static void xdg_toplevel_configure_handler(void *data, struct xdg_toplevel *xdg_toplevel,
                                           int32_t width, int32_t height,
                                           struct wl_array *states) {
    ocfx_window_t *window = data;
    (void)xdg_toplevel;
    (void)states;

    if (width > 0 && height > 0) {
        window->width = width;
        window->height = height;

        /* Resize EGL window if it exists */
        if (window->egl_window) {
            wl_egl_window_resize(window->egl_window, width, height, 0, 0);
        }

        /* Call user resize callback */
        if (window->resize_callback) {
            window->resize_callback(window, width, height, window->user_data);
        }
    }
}

static void xdg_toplevel_close_handler(void *data, struct xdg_toplevel *xdg_toplevel) {
    ocfx_window_t *window = data;
    (void)xdg_toplevel;

    window->should_close = true;

    /* Call user close callback */
    if (window->close_callback) {
        window->close_callback(window, window->user_data);
    }
}

/* ============================================================================
 * Seat Handlers
 * ============================================================================ */

static void seat_capabilities_handler(void *data, struct wl_seat *seat,
                                      uint32_t capabilities) {
    ocfx_window_t *window = data;

    /* Handle keyboard */
    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
        if (!window->keyboard) {
            window->keyboard = wl_seat_get_keyboard(seat);
            wl_keyboard_add_listener(window->keyboard, &keyboard_listener, window);
        }
    } else {
        if (window->keyboard) {
            wl_keyboard_destroy(window->keyboard);
            window->keyboard = NULL;
        }
    }

    /* Handle pointer */
    if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
        if (!window->pointer) {
            window->pointer = wl_seat_get_pointer(seat);
            wl_pointer_add_listener(window->pointer, &pointer_listener, window);
        }
    } else {
        if (window->pointer) {
            wl_pointer_destroy(window->pointer);
            window->pointer = NULL;
        }
    }
}

static void seat_name_handler(void *data, struct wl_seat *seat, const char *name) {
    (void)data;
    (void)seat;
    (void)name;
}

/* ============================================================================
 * Keyboard Handlers (Forward to input callbacks)
 * ============================================================================ */

static void keyboard_keymap_handler(void *data, struct wl_keyboard *keyboard,
                                   uint32_t format, int32_t fd, uint32_t size) {
    ocfx_window_t *window = data;
    (void)keyboard;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    char *map_str = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    window->xkb_keymap = xkb_keymap_new_from_string(window->xkb_context, map_str,
                                                     XKB_KEYMAP_FORMAT_TEXT_V1,
                                                     XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map_str, size);
    close(fd);

    if (!window->xkb_keymap) {
        return;
    }

    window->xkb_state = xkb_state_new(window->xkb_keymap);
}

static void keyboard_enter_handler(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface,
                                  struct wl_array *keys) {
    ocfx_window_t *window = data;
    (void)keyboard;
    (void)serial;
    (void)surface;
    (void)keys;

    /* Call focus callback */
    if (window->focus_callback) {
        window->focus_callback(window, true, window->user_data);
    }
}

static void keyboard_leave_handler(void *data, struct wl_keyboard *keyboard,
                                  uint32_t serial, struct wl_surface *surface) {
    ocfx_window_t *window = data;
    (void)keyboard;
    (void)serial;
    (void)surface;

    /* Call focus callback */
    if (window->focus_callback) {
        window->focus_callback(window, false, window->user_data);
    }
}

static void keyboard_key_handler(void *data, struct wl_keyboard *keyboard,
                                uint32_t serial, uint32_t time,
                                uint32_t key, uint32_t state_key) {
    ocfx_window_t *window = data;
    (void)keyboard;
    (void)serial;
    (void)time;

    if (!window->xkb_state) {
        return;
    }

    /* Get keysym */
    xkb_keysym_t keysym = xkb_state_key_get_one_sym(window->xkb_state, key + 8);

    /* Get UTF-8 character */
    char utf8[8] = {0};
    xkb_state_key_get_utf8(window->xkb_state, key + 8, utf8, sizeof(utf8));

    /* Build event and call callback (implemented in input.c) */
    ocfx_key_event_t event = {
        .key = keysym,
        .modifiers = window->modifiers,
        .is_repeat = false,  /* TODO: track repeats */
    };
    strncpy(event.utf8, utf8, sizeof(event.utf8) - 1);

    /* Call key callback if registered */
    if (window->key_callback && state_key == WL_KEYBOARD_KEY_STATE_PRESSED) {
        ((ocfx_key_callback_t)window->key_callback)(window, &event, window->user_data);
    }
}

static void keyboard_modifiers_handler(void *data, struct wl_keyboard *keyboard,
                                      uint32_t serial, uint32_t mods_depressed,
                                      uint32_t mods_latched, uint32_t mods_locked,
                                      uint32_t group) {
    ocfx_window_t *window = data;
    (void)keyboard;
    (void)serial;

    if (!window->xkb_state) {
        return;
    }

    xkb_state_update_mask(window->xkb_state, mods_depressed, mods_latched,
                          mods_locked, 0, 0, group);

    /* Update modifiers state */
    window->modifiers = 0;
    if (xkb_state_mod_name_is_active(window->xkb_state, XKB_MOD_NAME_SHIFT,
                                     XKB_STATE_MODS_EFFECTIVE)) {
        window->modifiers |= OCFX_MOD_SHIFT;
    }
    if (xkb_state_mod_name_is_active(window->xkb_state, XKB_MOD_NAME_CTRL,
                                     XKB_STATE_MODS_EFFECTIVE)) {
        window->modifiers |= OCFX_MOD_CTRL;
    }
    if (xkb_state_mod_name_is_active(window->xkb_state, XKB_MOD_NAME_ALT,
                                     XKB_STATE_MODS_EFFECTIVE)) {
        window->modifiers |= OCFX_MOD_ALT;
    }
    if (xkb_state_mod_name_is_active(window->xkb_state, XKB_MOD_NAME_LOGO,
                                     XKB_STATE_MODS_EFFECTIVE)) {
        window->modifiers |= OCFX_MOD_SUPER;
    }
}

static void keyboard_repeat_info_handler(void *data, struct wl_keyboard *keyboard,
                                        int32_t rate, int32_t delay) {
    ocfx_window_t *window = data;
    (void)keyboard;

    window->repeat_rate = rate;
    window->repeat_delay = delay;

    printf("Key repeat configured: rate=%d, delay=%d\n", rate, delay);
}

/* ============================================================================
 * Pointer Handlers
 * ============================================================================ */

static void pointer_enter_handler(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface,
                                 wl_fixed_t surface_x, wl_fixed_t surface_y) {
    ocfx_window_t *window = data;
    (void)pointer;
    (void)serial;
    (void)surface;

    window->mouse_x = wl_fixed_to_double(surface_x);
    window->mouse_y = wl_fixed_to_double(surface_y);
}

static void pointer_leave_handler(void *data, struct wl_pointer *pointer,
                                 uint32_t serial, struct wl_surface *surface) {
    (void)data;
    (void)pointer;
    (void)serial;
    (void)surface;
}

static void pointer_motion_handler(void *data, struct wl_pointer *pointer,
                                  uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    ocfx_window_t *window = data;
    (void)pointer;
    (void)time;

    double new_x = wl_fixed_to_double(surface_x);
    double new_y = wl_fixed_to_double(surface_y);

    /* Build mouse event */
    ocfx_mouse_event_t event = {
        .x = new_x,
        .y = new_y,
        .dx = new_x - window->mouse_x,
        .dy = new_y - window->mouse_y,
        .button = -1,
        .scroll_x = 0,
        .scroll_y = 0,
        .modifiers = window->modifiers,
    };

    window->mouse_x = new_x;
    window->mouse_y = new_y;

    /* Call mouse callback if registered */
    if (window->mouse_callback) {
        ((ocfx_mouse_callback_t)window->mouse_callback)(window, &event, window->user_data);
    }
}

static void pointer_button_handler(void *data, struct wl_pointer *pointer,
                                  uint32_t serial, uint32_t time, uint32_t button,
                                  uint32_t state_button) {
    ocfx_window_t *window = data;
    (void)pointer;
    (void)serial;
    (void)time;

    /* Update button state */
    int button_index = button - 0x110;  /* BTN_LEFT = 0x110 */
    if (button_index >= 0 && button_index < 8) {
        window->mouse_buttons[button_index] = (state_button == WL_POINTER_BUTTON_STATE_PRESSED);
    }

    /* Build mouse event */
    ocfx_mouse_event_t event = {
        .x = window->mouse_x,
        .y = window->mouse_y,
        .dx = 0,
        .dy = 0,
        .button = button_index,
        .scroll_x = 0,
        .scroll_y = 0,
        .modifiers = window->modifiers,
    };

    /* Call mouse callback if registered */
    if (window->mouse_callback) {
        ((ocfx_mouse_callback_t)window->mouse_callback)(window, &event, window->user_data);
    }
}

static void pointer_axis_handler(void *data, struct wl_pointer *pointer,
                                uint32_t time, uint32_t axis, wl_fixed_t value) {
    ocfx_window_t *window = data;
    (void)pointer;
    (void)time;

    double scroll_value = wl_fixed_to_double(value);

    /* Build mouse event */
    ocfx_mouse_event_t event = {
        .x = window->mouse_x,
        .y = window->mouse_y,
        .dx = 0,
        .dy = 0,
        .button = -1,
        .scroll_x = (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) ? scroll_value : 0,
        .scroll_y = (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) ? scroll_value : 0,
        .modifiers = window->modifiers,
    };

    /* Call mouse callback if registered */
    if (window->mouse_callback) {
        ((ocfx_mouse_callback_t)window->mouse_callback)(window, &event, window->user_data);
    }
}

static void pointer_frame_handler(void *data, struct wl_pointer *pointer) {
    (void)data;
    (void)pointer;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

ocfx_window_t* ocfx_window_create(const ocfx_window_config_t *config) {
    ocfx_window_t *window = calloc(1, sizeof(ocfx_window_t));
    if (!window) {
        return NULL;
    }

    /* Set initial state */
    window->width = config->width;
    window->height = config->height;
    window->user_data = config->user_data;
    window->title = strdup(config->title ? config->title : "OCFX Window");
    window->app_id = strdup(config->app_id ? config->app_id : (config->title ? config->title : "ocfx"));
    window->repeat_rate = 25;      /* Default: 25 keys/second */
    window->repeat_delay = 600;    /* Default: 600ms initial delay */

    /* Initialize XKB */
    window->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!window->xkb_context) {
        fprintf(stderr, "Failed to create XKB context\n");
        free(window->title);
        free(window);
        return NULL;
    }

    /* Connect to Wayland */
    window->display = wl_display_connect(NULL);
    if (!window->display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        xkb_context_unref(window->xkb_context);
        free(window->title);
        free(window);
        return NULL;
    }

    /* Get registry */
    window->registry = wl_display_get_registry(window->display);
    if (!window->registry) {
        fprintf(stderr, "Failed to get Wayland registry\n");
        wl_display_disconnect(window->display);
        xkb_context_unref(window->xkb_context);
        free(window->title);
        free(window);
        return NULL;
    }

    /* Add registry listener and roundtrip */
    wl_registry_add_listener(window->registry, &registry_listener, window);
    wl_display_roundtrip(window->display);

    /* Verify we got essential interfaces */
    if (!window->compositor || !window->xdg_wm_base) {
        fprintf(stderr, "Missing essential Wayland interfaces\n");
        ocfx_window_destroy(window);
        return NULL;
    }

    /* Create surface */
    window->surface = wl_compositor_create_surface(window->compositor);
    if (!window->surface) {
        fprintf(stderr, "Failed to create surface\n");
        ocfx_window_destroy(window);
        return NULL;
    }

    /* Create XDG surface */
    window->xdg_surface = xdg_wm_base_get_xdg_surface(window->xdg_wm_base,
                                                       window->surface);
    if (!window->xdg_surface) {
        fprintf(stderr, "Failed to create XDG surface\n");
        ocfx_window_destroy(window);
        return NULL;
    }
    xdg_surface_add_listener(window->xdg_surface, &xdg_surface_listener, window);

    /* Create XDG toplevel */
    window->xdg_toplevel = xdg_surface_get_toplevel(window->xdg_surface);
    if (!window->xdg_toplevel) {
        fprintf(stderr, "Failed to create XDG toplevel\n");
        ocfx_window_destroy(window);
        return NULL;
    }
    xdg_toplevel_add_listener(window->xdg_toplevel, &xdg_toplevel_listener, window);
    xdg_toplevel_set_title(window->xdg_toplevel, window->title);
    xdg_toplevel_set_app_id(window->xdg_toplevel, window->app_id);

    /* Create EGL window */
    window->egl_window = wl_egl_window_create(window->surface, window->width,
                                               window->height);
    if (!window->egl_window) {
        fprintf(stderr, "Failed to create EGL window\n");
        ocfx_window_destroy(window);
        return NULL;
    }

    /* Commit surface */
    wl_surface_commit(window->surface);

    /* Wait for configure */
    while (!window->configured) {
        wl_display_dispatch(window->display);
    }

    return window;
}

void ocfx_window_destroy(ocfx_window_t *window) {
    if (!window) return;

    if (window->egl_window) wl_egl_window_destroy(window->egl_window);
    if (window->xdg_toplevel) xdg_toplevel_destroy(window->xdg_toplevel);
    if (window->xdg_surface) xdg_surface_destroy(window->xdg_surface);
    if (window->surface) wl_surface_destroy(window->surface);
    if (window->keyboard) wl_keyboard_destroy(window->keyboard);
    if (window->pointer) wl_pointer_destroy(window->pointer);
    if (window->seat) wl_seat_destroy(window->seat);
    if (window->xdg_wm_base) xdg_wm_base_destroy(window->xdg_wm_base);
    if (window->compositor) wl_compositor_destroy(window->compositor);
    if (window->registry) wl_registry_destroy(window->registry);
    if (window->xkb_state) xkb_state_unref(window->xkb_state);
    if (window->xkb_keymap) xkb_keymap_unref(window->xkb_keymap);
    if (window->xkb_context) xkb_context_unref(window->xkb_context);
    if (window->display) wl_display_disconnect(window->display);
    if (window->title) free(window->title);
    if (window->app_id) free(window->app_id);

    free(window);
}

/* Window properties */
void ocfx_window_set_title(ocfx_window_t *window, const char *title) {
    if (!window || !title) return;

    free(window->title);
    window->title = strdup(title);

    if (window->xdg_toplevel) {
        xdg_toplevel_set_title(window->xdg_toplevel, title);
    }
}

void ocfx_window_set_app_id(ocfx_window_t *window, const char *app_id) {
    if (!window || !app_id) return;

    free(window->app_id);
    window->app_id = strdup(app_id);

    if (window->xdg_toplevel) {
        xdg_toplevel_set_app_id(window->xdg_toplevel, app_id);
    }
}

void ocfx_window_get_size(ocfx_window_t *window, int32_t *width, int32_t *height) {
    if (!window) return;
    if (width) *width = window->width;
    if (height) *height = window->height;
}

void ocfx_window_set_size(ocfx_window_t *window, int32_t width, int32_t height) {
    if (!window) return;

    window->width = width;
    window->height = height;

    if (window->egl_window) {
        wl_egl_window_resize(window->egl_window, width, height, 0, 0);
    }
}

bool ocfx_window_is_configured(ocfx_window_t *window) {
    return window ? window->configured : false;
}

void* ocfx_window_get_user_data(ocfx_window_t *window) {
    return window ? window->user_data : NULL;
}

/* Callbacks */
void ocfx_window_set_resize_callback(ocfx_window_t *window, ocfx_resize_callback_t callback) {
    if (window) window->resize_callback = callback;
}

void ocfx_window_set_close_callback(ocfx_window_t *window, ocfx_close_callback_t callback) {
    if (window) window->close_callback = callback;
}

void ocfx_window_set_focus_callback(ocfx_window_t *window, ocfx_focus_callback_t callback) {
    if (window) window->focus_callback = callback;
}

/* Event loop */
int ocfx_window_dispatch(ocfx_window_t *window) {
    if (!window || !window->display) return -1;

    /* Flush any pending requests first */
    wl_display_flush(window->display);

    /* Process pending events without blocking */
    wl_display_dispatch_pending(window->display);

    /* Check for errors */
    return wl_display_get_error(window->display);
}

bool ocfx_window_should_close(ocfx_window_t *window) {
    return window ? window->should_close : true;
}

void ocfx_window_request_close(ocfx_window_t *window) {
    if (window) window->should_close = true;
}

/* Low-level access */
struct wl_display* ocfx_window_get_wl_display(ocfx_window_t *window) {
    return window ? window->display : NULL;
}

struct wl_surface* ocfx_window_get_wl_surface(ocfx_window_t *window) {
    return window ? window->surface : NULL;
}

/* Internal function for EGL access (used by render.c) */
struct wl_egl_window* ocfx_window_get_egl_window(ocfx_window_t *window) {
    return window ? window->egl_window : NULL;
}

/* Internal functions for input.c */
void ocfx_window_set_key_callback_internal(ocfx_window_t *window, void *callback) {
    if (window) window->key_callback = callback;
}

void ocfx_window_set_mouse_callback_internal(ocfx_window_t *window, void *callback) {
    if (window) window->mouse_callback = callback;
}

bool ocfx_window_get_mouse_button_state(ocfx_window_t *window, int button) {
    if (!window || button < 0 || button >= 8) return false;
    return window->mouse_buttons[button];
}

void ocfx_window_get_mouse_position_internal(ocfx_window_t *window, double *x, double *y) {
    if (!window) return;
    if (x) *x = window->mouse_x;
    if (y) *y = window->mouse_y;
}

uint32_t ocfx_window_get_modifiers_internal(ocfx_window_t *window) {
    return window ? window->modifiers : 0;
}
