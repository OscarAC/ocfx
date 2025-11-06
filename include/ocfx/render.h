/* OCFX - OpenGL Rendering Interface
 * GPU-accelerated 2D rendering primitives
 */

#ifndef OCFX_RENDER_H
#define OCFX_RENDER_H

#include "types.h"
#include "wayland.h"
#include <GLES3/gl3.h>

/* Forward declaration */
typedef struct ocfx_renderer_t ocfx_renderer_t;

/* Renderer creation */
ocfx_renderer_t* ocfx_renderer_create(ocfx_window_t *window);
void ocfx_renderer_destroy(ocfx_renderer_t *renderer);

/* Frame management */
void ocfx_render_begin(ocfx_renderer_t *renderer, ocfx_color_t clear_color);
void ocfx_render_end(ocfx_renderer_t *renderer);
void ocfx_render_present(ocfx_renderer_t *renderer);

/* Viewport */
void ocfx_render_set_viewport(ocfx_renderer_t *renderer, int32_t width, int32_t height);
void ocfx_render_get_viewport(ocfx_renderer_t *renderer, int32_t *width, int32_t *height);

/* Drawing primitives */
void ocfx_draw_rect_filled(ocfx_renderer_t *renderer, ocfx_rect_t rect, ocfx_color_t color);
void ocfx_draw_rect_outline(ocfx_renderer_t *renderer, ocfx_rect_t rect, ocfx_color_t color, float thickness);
void ocfx_draw_line(ocfx_renderer_t *renderer, ocfx_point_t start, ocfx_point_t end,
                    ocfx_color_t color, float thickness);
void ocfx_draw_circle_filled(ocfx_renderer_t *renderer, ocfx_point_t center, float radius,
                              ocfx_color_t color);
void ocfx_draw_circle_outline(ocfx_renderer_t *renderer, ocfx_point_t center, float radius,
                               ocfx_color_t color, float thickness);

/* Advanced drawing */
void ocfx_draw_triangle_filled(ocfx_renderer_t *renderer, ocfx_point_t p1, ocfx_point_t p2,
                                ocfx_point_t p3, ocfx_color_t color);
void ocfx_draw_quad_filled(ocfx_renderer_t *renderer, ocfx_point_t p1, ocfx_point_t p2,
                            ocfx_point_t p3, ocfx_point_t p4, ocfx_color_t color);

/* Texture support (for images, text atlas, etc.) */
typedef struct ocfx_texture_t ocfx_texture_t;

ocfx_texture_t* ocfx_texture_create(ocfx_renderer_t *renderer, int width, int height,
                                     const uint8_t *data);
void ocfx_texture_destroy(ocfx_texture_t *texture);
void ocfx_texture_update(ocfx_texture_t *texture, int x, int y, int width, int height,
                         const uint8_t *data);
void ocfx_draw_texture(ocfx_renderer_t *renderer, ocfx_texture_t *texture,
                       ocfx_rect_t src, ocfx_rect_t dst, ocfx_color_t tint);

/* State management */
void ocfx_render_push_clip(ocfx_renderer_t *renderer, ocfx_rect_t clip);
void ocfx_render_pop_clip(ocfx_renderer_t *renderer);
void ocfx_render_set_blend_mode(ocfx_renderer_t *renderer, bool enabled);

/* Low-level access */
GLuint ocfx_renderer_get_shader(ocfx_renderer_t *renderer, const char *name);

#endif /* OCFX_RENDER_H */
