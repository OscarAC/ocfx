/* OCFX - Input Handling Interface
 * Keyboard and mouse input events
 */

#ifndef OCFX_INPUT_H
#define OCFX_INPUT_H

#include "types.h"
#include "wayland.h"
#include <xkbcommon/xkbcommon.h>

/* Key modifiers */
typedef enum {
    OCFX_MOD_NONE  = 0,
    OCFX_MOD_SHIFT = (1 << 0),
    OCFX_MOD_CTRL  = (1 << 1),
    OCFX_MOD_ALT   = (1 << 2),
    OCFX_MOD_SUPER = (1 << 3),
} ocfx_modifier_t;

/* Mouse buttons */
typedef enum {
    OCFX_MOUSE_BUTTON_LEFT = 0,
    OCFX_MOUSE_BUTTON_RIGHT = 1,
    OCFX_MOUSE_BUTTON_MIDDLE = 2,
} ocfx_mouse_button_t;

/* Event types */
typedef enum {
    OCFX_EVENT_KEY_PRESS,
    OCFX_EVENT_KEY_RELEASE,
    OCFX_EVENT_MOUSE_MOVE,
    OCFX_EVENT_MOUSE_BUTTON_PRESS,
    OCFX_EVENT_MOUSE_BUTTON_RELEASE,
    OCFX_EVENT_MOUSE_SCROLL,
} ocfx_event_type_t;

/* Keyboard event */
typedef struct {
    uint32_t key;              /* XKB keysym (XKB_KEY_*) */
    uint32_t modifiers;        /* ocfx_modifier_t flags */
    char utf8[8];              /* UTF-8 representation (if printable) */
    bool is_repeat;            /* Is this a repeated key? */
} ocfx_key_event_t;

/* Mouse event */
typedef struct {
    double x, y;               /* Position in window coordinates */
    double dx, dy;             /* Delta movement (for MOUSE_MOVE) */
    int button;                /* Button index (for BUTTON events) */
    double scroll_x, scroll_y; /* Scroll amount (for SCROLL) */
    uint32_t modifiers;        /* ocfx_modifier_t flags */
} ocfx_mouse_event_t;

/* Event union */
typedef struct {
    ocfx_event_type_t type;
    union {
        ocfx_key_event_t key;
        ocfx_mouse_event_t mouse;
    };
} ocfx_event_t;

/* Event callbacks */
typedef void (*ocfx_key_callback_t)(ocfx_window_t *window, const ocfx_key_event_t *event, void *user_data);
typedef void (*ocfx_mouse_callback_t)(ocfx_window_t *window, const ocfx_mouse_event_t *event, void *user_data);

/* Register callbacks */
void ocfx_input_set_key_callback(ocfx_window_t *window, ocfx_key_callback_t callback);
void ocfx_input_set_mouse_callback(ocfx_window_t *window, ocfx_mouse_callback_t callback);

/* Input state queries */
bool ocfx_input_is_key_down(ocfx_window_t *window, uint32_t key);
bool ocfx_input_is_mouse_button_down(ocfx_window_t *window, int button);
void ocfx_input_get_mouse_position(ocfx_window_t *window, double *x, double *y);
uint32_t ocfx_input_get_modifiers(ocfx_window_t *window);

/* Key utilities */
const char* ocfx_key_name(uint32_t keysym);
bool ocfx_key_is_printable(uint32_t keysym);
uint32_t ocfx_key_from_name(const char *name);

/* Re-export common XKB keys for convenience */
#define OCFX_KEY_ESCAPE     XKB_KEY_Escape
#define OCFX_KEY_RETURN     XKB_KEY_Return
#define OCFX_KEY_TAB        XKB_KEY_Tab
#define OCFX_KEY_BACKSPACE  XKB_KEY_BackSpace
#define OCFX_KEY_DELETE     XKB_KEY_Delete
#define OCFX_KEY_LEFT       XKB_KEY_Left
#define OCFX_KEY_RIGHT      XKB_KEY_Right
#define OCFX_KEY_UP         XKB_KEY_Up
#define OCFX_KEY_DOWN       XKB_KEY_Down
#define OCFX_KEY_HOME       XKB_KEY_Home
#define OCFX_KEY_END        XKB_KEY_End
#define OCFX_KEY_PAGE_UP    XKB_KEY_Page_Up
#define OCFX_KEY_PAGE_DOWN  XKB_KEY_Page_Down
#define OCFX_KEY_SPACE      XKB_KEY_space

#endif /* OCFX_INPUT_H */
