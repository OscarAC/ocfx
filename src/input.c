/* OCFX - Input Handling Implementation
 */

#include "ocfx/input.h"
#include "ocfx/wayland.h"
#include <string.h>
#include <xkbcommon/xkbcommon-names.h>

/* Internal window structure access (defined in wayland.c) */
struct ocfx_window_t;

/* Input callbacks are set via these functions */
void ocfx_input_set_key_callback(ocfx_window_t *window, ocfx_key_callback_t callback) {
    if (!window) return;
    /* Store callback in window->key_callback (accessed via void* in wayland.c) */
    extern void ocfx_window_set_key_callback_internal(ocfx_window_t *window, void *callback);
    ocfx_window_set_key_callback_internal(window, (void*)callback);
}

void ocfx_input_set_mouse_callback(ocfx_window_t *window, ocfx_mouse_callback_t callback) {
    if (!window) return;
    extern void ocfx_window_set_mouse_callback_internal(ocfx_window_t *window, void *callback);
    ocfx_window_set_mouse_callback_internal(window, (void*)callback);
}

/* Input state queries (to be implemented with window struct access) */
bool ocfx_input_is_key_down(ocfx_window_t *window, uint32_t key) {
    (void)window;
    (void)key;
    /* TODO: Track key state */
    return false;
}

bool ocfx_input_is_mouse_button_down(ocfx_window_t *window, int button) {
    if (!window || button < 0 || button >= 8) return false;
    extern bool ocfx_window_get_mouse_button_state(ocfx_window_t *window, int button);
    return ocfx_window_get_mouse_button_state(window, button);
}

void ocfx_input_get_mouse_position(ocfx_window_t *window, double *x, double *y) {
    if (!window) return;
    extern void ocfx_window_get_mouse_position_internal(ocfx_window_t *window, double *x, double *y);
    ocfx_window_get_mouse_position_internal(window, x, y);
}

uint32_t ocfx_input_get_modifiers(ocfx_window_t *window) {
    if (!window) return 0;
    extern uint32_t ocfx_window_get_modifiers_internal(ocfx_window_t *window);
    return ocfx_window_get_modifiers_internal(window);
}

/* Key utilities */
const char* ocfx_key_name(uint32_t keysym) {
    static char buffer[64];
    xkb_keysym_get_name(keysym, buffer, sizeof(buffer));
    return buffer;
}

bool ocfx_key_is_printable(uint32_t keysym) {
    /* Check if keysym is in printable range */
    return (keysym >= 0x20 && keysym < 0x7F) ||  /* ASCII printable */
           (keysym >= 0xA0 && keysym < 0x100);    /* Latin-1 supplement */
}

uint32_t ocfx_key_from_name(const char *name) {
    if (!name) return XKB_KEY_NoSymbol;
    return xkb_keysym_from_name(name, XKB_KEYSYM_NO_FLAGS);
}
