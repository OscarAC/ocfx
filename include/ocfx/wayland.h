/* OCFX - Wayland Client Interface
 * Wayland window management and protocol handling
 */

#ifndef OCFX_WAYLAND_H
#define OCFX_WAYLAND_H

#include "types.h"
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

/* Forward declarations for opaque types */
typedef struct ocfx_window_t ocfx_window_t;

/* Window configuration */
typedef struct {
    const char *title;           /* Window title */
    int32_t width;               /* Initial width */
    int32_t height;              /* Initial height */
    bool resizable;              /* Allow resizing */
    bool decorated;              /* Show decorations (if compositor supports) */
    void *user_data;             /* User-defined data pointer */
} ocfx_window_config_t;

/* Event callbacks */
typedef void (*ocfx_resize_callback_t)(ocfx_window_t *window, int32_t width, int32_t height, void *user_data);
typedef void (*ocfx_close_callback_t)(ocfx_window_t *window, void *user_data);
typedef void (*ocfx_focus_callback_t)(ocfx_window_t *window, bool focused, void *user_data);

/* Window management */
ocfx_window_t* ocfx_window_create(const ocfx_window_config_t *config);
void ocfx_window_destroy(ocfx_window_t *window);

/* Window properties */
void ocfx_window_set_title(ocfx_window_t *window, const char *title);
void ocfx_window_get_size(ocfx_window_t *window, int32_t *width, int32_t *height);
void ocfx_window_set_size(ocfx_window_t *window, int32_t width, int32_t height);
bool ocfx_window_is_configured(ocfx_window_t *window);
void* ocfx_window_get_user_data(ocfx_window_t *window);

/* Event callbacks */
void ocfx_window_set_resize_callback(ocfx_window_t *window, ocfx_resize_callback_t callback);
void ocfx_window_set_close_callback(ocfx_window_t *window, ocfx_close_callback_t callback);
void ocfx_window_set_focus_callback(ocfx_window_t *window, ocfx_focus_callback_t callback);

/* Event loop */
int ocfx_window_dispatch(ocfx_window_t *window);  /* Returns -1 on error, 0 on quit */
bool ocfx_window_should_close(ocfx_window_t *window);
void ocfx_window_request_close(ocfx_window_t *window);

/* Low-level access (for advanced use cases) */
struct wl_display* ocfx_window_get_wl_display(ocfx_window_t *window);
struct wl_surface* ocfx_window_get_wl_surface(ocfx_window_t *window);

#endif /* OCFX_WAYLAND_H */
