/* OCFX - Text Rendering Interface
 * FreeType-based font rendering with GPU texture atlas
 */

#ifndef OCFX_TEXT_H
#define OCFX_TEXT_H

#include "types.h"
#include "render.h"

/* Forward declaration */
typedef struct ocfx_font_t ocfx_font_t;

/* Font loading */
ocfx_font_t* ocfx_font_load(ocfx_renderer_t *renderer, const char *font_path, int size);
ocfx_font_t* ocfx_font_load_system(ocfx_renderer_t *renderer, const char *font_name, int size);
void ocfx_font_destroy(ocfx_font_t *font);

/* Font metrics */
int ocfx_font_get_height(ocfx_font_t *font);
int ocfx_font_get_advance(ocfx_font_t *font);
int ocfx_font_get_ascent(ocfx_font_t *font);
int ocfx_font_get_descent(ocfx_font_t *font);

/* Text measurement */
void ocfx_text_measure(ocfx_font_t *font, const char *text, float *width, float *height);
void ocfx_text_measure_n(ocfx_font_t *font, const char *text, size_t len,
                         float *width, float *height);

/* Text rendering */
void ocfx_text_draw(ocfx_renderer_t *renderer, ocfx_font_t *font,
                    const char *text, float x, float y, ocfx_color_t color);
void ocfx_text_draw_n(ocfx_renderer_t *renderer, ocfx_font_t *font,
                      const char *text, size_t len, float x, float y, ocfx_color_t color);

/* Advanced text rendering */
typedef enum {
    OCFX_TEXT_ALIGN_LEFT,
    OCFX_TEXT_ALIGN_CENTER,
    OCFX_TEXT_ALIGN_RIGHT,
} ocfx_text_align_t;

typedef enum {
    OCFX_TEXT_BASELINE_TOP,
    OCFX_TEXT_BASELINE_MIDDLE,
    OCFX_TEXT_BASELINE_BOTTOM,
    OCFX_TEXT_BASELINE_ALPHABETIC,
} ocfx_text_baseline_t;

void ocfx_text_draw_aligned(ocfx_renderer_t *renderer, ocfx_font_t *font,
                            const char *text, ocfx_rect_t rect,
                            ocfx_text_align_t align, ocfx_text_baseline_t baseline,
                            ocfx_color_t color);

/* Multi-line text */
void ocfx_text_draw_wrapped(ocfx_renderer_t *renderer, ocfx_font_t *font,
                            const char *text, ocfx_rect_t rect, float line_spacing,
                            ocfx_color_t color);

/* UTF-8 support */
bool ocfx_text_is_valid_utf8(const char *text, size_t len);
size_t ocfx_text_utf8_char_count(const char *text);

#endif /* OCFX_TEXT_H */
