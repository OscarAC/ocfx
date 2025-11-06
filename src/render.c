/* OCFX - OpenGL Rendering Implementation
 * GPU-accelerated 2D rendering
 */

#include "ocfx/render.h"
#include "ocfx/wayland.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

/* Forward declaration from wayland.c */
extern struct wl_egl_window* ocfx_window_get_egl_window(ocfx_window_t *window);

/* Renderer structure (opaque to users) */
struct ocfx_renderer_t {
    ocfx_window_t *window;

    /* EGL */
    EGLDisplay egl_display;
    EGLContext egl_context;
    EGLSurface egl_surface;
    EGLConfig egl_config;

    /* OpenGL */
    GLuint basic_shader;  /* For rectangles, primitives */
    GLuint vao;
    GLuint vbo;

    /* Viewport */
    int32_t viewport_width;
    int32_t viewport_height;
};

/* Basic vertex shader */
static const char *basic_vertex_shader =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location = 0) in vec2 a_position;\n"
    "layout(location = 1) in vec4 a_color;\n"
    "out vec4 v_color;\n"
    "uniform vec2 u_resolution;\n"
    "void main() {\n"
    "    vec2 clip_pos = (a_position / u_resolution) * 2.0 - 1.0;\n"
    "    clip_pos.y = -clip_pos.y;\n"
    "    gl_Position = vec4(clip_pos, 0.0, 1.0);\n"
    "    v_color = a_color;\n"
    "}\n";

/* Basic fragment shader */
static const char *basic_fragment_shader =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec4 v_color;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = v_color;\n"
    "}\n";

/* Compile shader */
static GLuint compile_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, sizeof(info_log), NULL, info_log);
        fprintf(stderr, "OCFX: Shader compilation failed: %s\n", info_log);
        return 0;
    }
    return shader;
}

/* Create shader program */
static GLuint create_shader_program(const char *vert_src, const char *frag_src) {
    GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src);

    if (!vert || !frag) {
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, sizeof(info_log), NULL, info_log);
        fprintf(stderr, "OCFX: Program linking failed: %s\n", info_log);
        return 0;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    return program;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

ocfx_renderer_t* ocfx_renderer_create(ocfx_window_t *window) {
    if (!window) return NULL;

    ocfx_renderer_t *renderer = calloc(1, sizeof(ocfx_renderer_t));
    if (!renderer) return NULL;

    renderer->window = window;

    /* Get window size */
    ocfx_window_get_size(window, &renderer->viewport_width, &renderer->viewport_height);

    /* EGL configuration */
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE
    };

    const EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    /* Get EGL display */
    struct wl_display *wl_display = ocfx_window_get_wl_display(window);
    renderer->egl_display = eglGetDisplay((EGLNativeDisplayType)wl_display);
    if (renderer->egl_display == EGL_NO_DISPLAY) {
        fprintf(stderr, "OCFX: Failed to get EGL display\n");
        free(renderer);
        return NULL;
    }

    /* Initialize EGL */
    EGLint major, minor;
    if (!eglInitialize(renderer->egl_display, &major, &minor)) {
        fprintf(stderr, "OCFX: Failed to initialize EGL\n");
        free(renderer);
        return NULL;
    }

    /* Choose config */
    EGLint config_count;
    if (!eglChooseConfig(renderer->egl_display, config_attribs,
                         &renderer->egl_config, 1, &config_count) || config_count == 0) {
        fprintf(stderr, "OCFX: Failed to choose EGL config\n");
        eglTerminate(renderer->egl_display);
        free(renderer);
        return NULL;
    }

    /* Create EGL context */
    renderer->egl_context = eglCreateContext(renderer->egl_display,
                                             renderer->egl_config,
                                             EGL_NO_CONTEXT,
                                             context_attribs);
    if (renderer->egl_context == EGL_NO_CONTEXT) {
        fprintf(stderr, "OCFX: Failed to create EGL context\n");
        eglTerminate(renderer->egl_display);
        free(renderer);
        return NULL;
    }

    /* Create EGL surface */
    struct wl_egl_window *egl_window = ocfx_window_get_egl_window(window);
    renderer->egl_surface = eglCreateWindowSurface(renderer->egl_display,
                                                    renderer->egl_config,
                                                    (EGLNativeWindowType)egl_window,
                                                    NULL);
    if (renderer->egl_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "OCFX: Failed to create EGL surface\n");
        eglDestroyContext(renderer->egl_display, renderer->egl_context);
        eglTerminate(renderer->egl_display);
        free(renderer);
        return NULL;
    }

    /* Make context current */
    if (!eglMakeCurrent(renderer->egl_display, renderer->egl_surface,
                        renderer->egl_surface, renderer->egl_context)) {
        fprintf(stderr, "OCFX: Failed to make EGL context current\n");
        eglDestroySurface(renderer->egl_display, renderer->egl_surface);
        eglDestroyContext(renderer->egl_display, renderer->egl_context);
        eglTerminate(renderer->egl_display);
        free(renderer);
        return NULL;
    }

    /* Create shader program */
    renderer->basic_shader = create_shader_program(basic_vertex_shader,
                                                    basic_fragment_shader);
    if (!renderer->basic_shader) {
        fprintf(stderr, "OCFX: Failed to create shader program\n");
        ocfx_renderer_destroy(renderer);
        return NULL;
    }

    /* Create VAO and VBO */
    glGenVertexArrays(1, &renderer->vao);
    glGenBuffers(1, &renderer->vbo);

    /* Set up OpenGL state */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, renderer->viewport_width, renderer->viewport_height);

    return renderer;
}

void ocfx_renderer_destroy(ocfx_renderer_t *renderer) {
    if (!renderer) return;

    if (renderer->vbo) glDeleteBuffers(1, &renderer->vbo);
    if (renderer->vao) glDeleteVertexArrays(1, &renderer->vao);
    if (renderer->basic_shader) glDeleteProgram(renderer->basic_shader);

    if (renderer->egl_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(renderer->egl_display, EGL_NO_SURFACE,
                       EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (renderer->egl_surface) {
            eglDestroySurface(renderer->egl_display, renderer->egl_surface);
        }
        if (renderer->egl_context) {
            eglDestroyContext(renderer->egl_display, renderer->egl_context);
        }
        eglTerminate(renderer->egl_display);
    }

    free(renderer);
}

/* Frame management */
void ocfx_render_begin(ocfx_renderer_t *renderer, ocfx_color_t clear_color) {
    if (!renderer) return;

    glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void ocfx_render_end(ocfx_renderer_t *renderer) {
    if (!renderer) return;
    glFlush();
}

void ocfx_render_present(ocfx_renderer_t *renderer) {
    if (!renderer) return;
    eglSwapBuffers(renderer->egl_display, renderer->egl_surface);
}

/* Viewport */
void ocfx_render_set_viewport(ocfx_renderer_t *renderer, int32_t width, int32_t height) {
    if (!renderer) return;
    renderer->viewport_width = width;
    renderer->viewport_height = height;
    glViewport(0, 0, width, height);
}

void ocfx_render_get_viewport(ocfx_renderer_t *renderer, int32_t *width, int32_t *height) {
    if (!renderer) return;
    if (width) *width = renderer->viewport_width;
    if (height) *height = renderer->viewport_height;
}

/* Drawing primitives */
void ocfx_draw_rect_filled(ocfx_renderer_t *renderer, ocfx_rect_t rect, ocfx_color_t color) {
    if (!renderer) return;

    /* Vertex data: position (x, y) + color (r, g, b, a) */
    float vertices[] = {
        /* Triangle 1 */
        rect.x,             rect.y,              color.r, color.g, color.b, color.a,
        rect.x + rect.width, rect.y,              color.r, color.g, color.b, color.a,
        rect.x,             rect.y + rect.height, color.r, color.g, color.b, color.a,
        /* Triangle 2 */
        rect.x + rect.width, rect.y,              color.r, color.g, color.b, color.a,
        rect.x + rect.width, rect.y + rect.height, color.r, color.g, color.b, color.a,
        rect.x,             rect.y + rect.height, color.r, color.g, color.b, color.a,
    };

    glUseProgram(renderer->basic_shader);

    /* Set resolution uniform */
    GLint u_resolution = glGetUniformLocation(renderer->basic_shader, "u_resolution");
    glUniform2f(u_resolution, (float)renderer->viewport_width, (float)renderer->viewport_height);

    /* Bind VAO and VBO */
    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    /* Set up vertex attributes */
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    /* Draw */
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
}

void ocfx_draw_rect_outline(ocfx_renderer_t *renderer, ocfx_rect_t rect,
                              ocfx_color_t color, float thickness) {
    if (!renderer) return;

    /* Draw 4 rectangles for the outline */
    ocfx_draw_rect_filled(renderer, OCFX_RECT(rect.x, rect.y, rect.width, thickness), color); /* Top */
    ocfx_draw_rect_filled(renderer, OCFX_RECT(rect.x, rect.y + rect.height - thickness,
                          rect.width, thickness), color); /* Bottom */
    ocfx_draw_rect_filled(renderer, OCFX_RECT(rect.x, rect.y, thickness, rect.height), color); /* Left */
    ocfx_draw_rect_filled(renderer, OCFX_RECT(rect.x + rect.width - thickness, rect.y,
                          thickness, rect.height), color); /* Right */
}

void ocfx_draw_line(ocfx_renderer_t *renderer, ocfx_point_t start, ocfx_point_t end,
                    ocfx_color_t color, float thickness) {
    if (!renderer) return;

    /* Calculate perpendicular offset for thickness */
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float len = sqrtf(dx * dx + dy * dy);

    if (len == 0) return;

    float nx = -dy / len * thickness * 0.5f;
    float ny = dx / len * thickness * 0.5f;

    /* Create quad for line */
    float vertices[] = {
        start.x + nx, start.y + ny, color.r, color.g, color.b, color.a,
        start.x - nx, start.y - ny, color.r, color.g, color.b, color.a,
        end.x + nx,   end.y + ny,   color.r, color.g, color.b, color.a,

        start.x - nx, start.y - ny, color.r, color.g, color.b, color.a,
        end.x - nx,   end.y - ny,   color.r, color.g, color.b, color.a,
        end.x + nx,   end.y + ny,   color.r, color.g, color.b, color.a,
    };

    glUseProgram(renderer->basic_shader);
    GLint u_resolution = glGetUniformLocation(renderer->basic_shader, "u_resolution");
    glUniform2f(u_resolution, (float)renderer->viewport_width, (float)renderer->viewport_height);

    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void ocfx_draw_circle_filled(ocfx_renderer_t *renderer, ocfx_point_t center,
                              float radius, ocfx_color_t color) {
    if (!renderer) return;

    const int segments = 32;
    const float angle_step = 2.0f * 3.14159265359f / segments;

    /* Generate vertices for triangle fan */
    float vertices[(segments + 2) * 6];  /* Center + segments + closing point */

    /* Center vertex */
    vertices[0] = center.x;
    vertices[1] = center.y;
    vertices[2] = color.r;
    vertices[3] = color.g;
    vertices[4] = color.b;
    vertices[5] = color.a;

    /* Circle vertices */
    for (int i = 0; i <= segments; i++) {
        float angle = i * angle_step;
        int idx = (i + 1) * 6;
        vertices[idx + 0] = center.x + cosf(angle) * radius;
        vertices[idx + 1] = center.y + sinf(angle) * radius;
        vertices[idx + 2] = color.r;
        vertices[idx + 3] = color.g;
        vertices[idx + 4] = color.b;
        vertices[idx + 5] = color.a;
    }

    glUseProgram(renderer->basic_shader);
    GLint u_resolution = glGetUniformLocation(renderer->basic_shader, "u_resolution");
    glUniform2f(u_resolution, (float)renderer->viewport_width, (float)renderer->viewport_height);

    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 2);
    glBindVertexArray(0);
}

void ocfx_draw_circle_outline(ocfx_renderer_t *renderer, ocfx_point_t center,
                               float radius, ocfx_color_t color, float thickness) {
    (void)thickness;  /* TODO: Implement proper thickness */

    if (!renderer) return;

    const int segments = 32;
    const float angle_step = 2.0f * 3.14159265359f / segments;

    /* Generate vertices for line loop */
    float vertices[(segments + 1) * 6];

    for (int i = 0; i <= segments; i++) {
        float angle = i * angle_step;
        int idx = i * 6;
        vertices[idx + 0] = center.x + cosf(angle) * radius;
        vertices[idx + 1] = center.y + sinf(angle) * radius;
        vertices[idx + 2] = color.r;
        vertices[idx + 3] = color.g;
        vertices[idx + 4] = color.b;
        vertices[idx + 5] = color.a;
    }

    glUseProgram(renderer->basic_shader);
    GLint u_resolution = glGetUniformLocation(renderer->basic_shader, "u_resolution");
    glUniform2f(u_resolution, (float)renderer->viewport_width, (float)renderer->viewport_height);

    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_LINE_STRIP, 0, segments + 1);
    glBindVertexArray(0);
}

/* Advanced drawing - triangles and quads */
void ocfx_draw_triangle_filled(ocfx_renderer_t *renderer, ocfx_point_t p1,
                                ocfx_point_t p2, ocfx_point_t p3, ocfx_color_t color) {
    if (!renderer) return;

    float vertices[] = {
        p1.x, p1.y, color.r, color.g, color.b, color.a,
        p2.x, p2.y, color.r, color.g, color.b, color.a,
        p3.x, p3.y, color.r, color.g, color.b, color.a,
    };

    glUseProgram(renderer->basic_shader);
    GLint u_resolution = glGetUniformLocation(renderer->basic_shader, "u_resolution");
    glUniform2f(u_resolution, (float)renderer->viewport_width, (float)renderer->viewport_height);

    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void ocfx_draw_quad_filled(ocfx_renderer_t *renderer, ocfx_point_t p1, ocfx_point_t p2,
                            ocfx_point_t p3, ocfx_point_t p4, ocfx_color_t color) {
    if (!renderer) return;

    float vertices[] = {
        p1.x, p1.y, color.r, color.g, color.b, color.a,
        p2.x, p2.y, color.r, color.g, color.b, color.a,
        p3.x, p3.y, color.r, color.g, color.b, color.a,

        p1.x, p1.y, color.r, color.g, color.b, color.a,
        p3.x, p3.y, color.r, color.g, color.b, color.a,
        p4.x, p4.y, color.r, color.g, color.b, color.a,
    };

    glUseProgram(renderer->basic_shader);
    GLint u_resolution = glGetUniformLocation(renderer->basic_shader, "u_resolution");
    glUniform2f(u_resolution, (float)renderer->viewport_width, (float)renderer->viewport_height);

    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

/* Texture support - stubs for now, will be fully implemented with text.c */
ocfx_texture_t* ocfx_texture_create(ocfx_renderer_t *renderer, int width, int height,
                                     const uint8_t *data) {
    (void)renderer;
    (void)width;
    (void)height;
    (void)data;
    /* TODO: Implement texture creation */
    return NULL;
}

void ocfx_texture_destroy(ocfx_texture_t *texture) {
    (void)texture;
    /* TODO: Implement texture destruction */
}

void ocfx_texture_update(ocfx_texture_t *texture, int x, int y, int width, int height,
                         const uint8_t *data) {
    (void)texture;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)data;
    /* TODO: Implement texture update */
}

void ocfx_draw_texture(ocfx_renderer_t *renderer, ocfx_texture_t *texture,
                       ocfx_rect_t src, ocfx_rect_t dst, ocfx_color_t tint) {
    (void)renderer;
    (void)texture;
    (void)src;
    (void)dst;
    (void)tint;
    /* TODO: Implement texture drawing */
}

/* State management */
void ocfx_render_push_clip(ocfx_renderer_t *renderer, ocfx_rect_t clip) {
    if (!renderer) return;
    glEnable(GL_SCISSOR_TEST);
    glScissor((GLint)clip.x, (GLint)(renderer->viewport_height - clip.y - clip.height),
              (GLsizei)clip.width, (GLsizei)clip.height);
}

void ocfx_render_pop_clip(ocfx_renderer_t *renderer) {
    (void)renderer;
    glDisable(GL_SCISSOR_TEST);
}

void ocfx_render_set_blend_mode(ocfx_renderer_t *renderer, bool enabled) {
    (void)renderer;
    if (enabled) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

/* Low-level access */
GLuint ocfx_renderer_get_shader(ocfx_renderer_t *renderer, const char *name) {
    if (!renderer || !name) return 0;

    if (strcmp(name, "basic") == 0) {
        return renderer->basic_shader;
    }

    return 0;
}
