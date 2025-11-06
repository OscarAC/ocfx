/* OCFX - Common Types
 * Fundamental types used across OCFX
 */

#ifndef OCFX_TYPES_H
#define OCFX_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Result codes */
typedef enum {
    OCFX_OK = 0,
    OCFX_ERROR = -1,
    OCFX_ERROR_INIT = -2,
    OCFX_ERROR_MEMORY = -3,
    OCFX_ERROR_INVALID = -4,
    OCFX_ERROR_NOT_FOUND = -5,
} ocfx_result_t;

/* Color (RGBA, 0.0-1.0 range) */
typedef struct {
    float r, g, b, a;
} ocfx_color_t;

/* 2D Point */
typedef struct {
    float x, y;
} ocfx_point_t;

/* 2D Size */
typedef struct {
    float width, height;
} ocfx_size_t;

/* Rectangle */
typedef struct {
    float x, y;
    float width, height;
} ocfx_rect_t;

/* Predefined colors */
extern const ocfx_color_t OCFX_COLOR_BLACK;
extern const ocfx_color_t OCFX_COLOR_WHITE;
extern const ocfx_color_t OCFX_COLOR_RED;
extern const ocfx_color_t OCFX_COLOR_GREEN;
extern const ocfx_color_t OCFX_COLOR_BLUE;
extern const ocfx_color_t OCFX_COLOR_TRANSPARENT;

/* Helper macros */
#define OCFX_COLOR_RGB(r, g, b) ((ocfx_color_t){r, g, b, 1.0f})
#define OCFX_COLOR_RGBA(r, g, b, a) ((ocfx_color_t){r, g, b, a})
#define OCFX_RECT(x, y, w, h) ((ocfx_rect_t){x, y, w, h})
#define OCFX_POINT(x, y) ((ocfx_point_t){x, y})
#define OCFX_SIZE(w, h) ((ocfx_size_t){w, h})

#endif /* OCFX_TYPES_H */
