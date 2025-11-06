/* OCFX - Text Rendering Implementation
 * FreeType-based font rendering with GPU texture atlas
 */

#include "ocfx/text.h"
#include "ocfx/render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <GLES3/gl3.h>

/* Glyph cache entry */
typedef struct {
    uint32_t codepoint;
    float atlas_x, atlas_y;      /* Position in atlas (normalized 0-1) */
    float atlas_width, atlas_height;  /* Size in atlas (normalized) */
    float width, height;          /* Size in pixels */
    float bearing_x, bearing_y;   /* Bearing in pixels */
    float advance;                /* Advance in pixels */
} glyph_cache_entry_t;

/* Font structure (opaque to users) */
struct ocfx_font_t {
    ocfx_renderer_t *renderer;

    /* FreeType */
    FT_Library ft_library;
    FT_Face ft_face;
    int size;

    /* Font metrics */
    int height;
    int advance;
    int ascent;
    int descent;

    /* GPU texture atlas */
    GLuint texture;
    int atlas_width;
    int atlas_height;
    int atlas_x;
    int atlas_y;
    int atlas_row_height;

    /* Glyph cache */
    glyph_cache_entry_t *glyph_cache;
    size_t glyph_cache_size;
    size_t glyph_cache_capacity;

    /* Shader program for text */
    GLuint shader_program;
    GLuint vao;
    GLuint vbo;
};

/* Text vertex shader */
static const char *text_vertex_shader =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location = 0) in vec2 a_position;\n"
    "layout(location = 1) in vec2 a_texcoord;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "out vec2 v_texcoord;\n"
    "out vec4 v_color;\n"
    "uniform vec2 u_resolution;\n"
    "void main() {\n"
    "    vec2 clip_pos = (a_position / u_resolution) * 2.0 - 1.0;\n"
    "    clip_pos.y = -clip_pos.y;\n"
    "    gl_Position = vec4(clip_pos, 0.0, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "    v_color = a_color;\n"
    "}\n";

/* Text fragment shader */
static const char *text_fragment_shader =
    "#version 300 es\n"
    "precision highp float;\n"
    "in vec2 v_texcoord;\n"
    "in vec4 v_color;\n"
    "out vec4 fragColor;\n"
    "uniform sampler2D u_texture;\n"
    "void main() {\n"
    "    float alpha = texture(u_texture, v_texcoord).r;\n"
    "    fragColor = vec4(v_color.rgb, v_color.a * alpha);\n"
    "}\n";

/* Compile shader (copied from render.c pattern) */
static GLuint compile_text_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, sizeof(info_log), NULL, info_log);
        fprintf(stderr, "OCFX: Text shader compilation failed: %s\n", info_log);
        return 0;
    }
    return shader;
}

/* Create text shader program */
static GLuint create_text_shader_program(void) {
    GLuint vert = compile_text_shader(GL_VERTEX_SHADER, text_vertex_shader);
    GLuint frag = compile_text_shader(GL_FRAGMENT_SHADER, text_fragment_shader);

    if (!vert || !frag) return 0;

    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, sizeof(info_log), NULL, info_log);
        fprintf(stderr, "OCFX: Text shader linking failed: %s\n", info_log);
        return 0;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    return program;
}

/* Find glyph in cache */
static glyph_cache_entry_t* find_glyph(ocfx_font_t *font, uint32_t codepoint) {
    for (size_t i = 0; i < font->glyph_cache_size; i++) {
        if (font->glyph_cache[i].codepoint == codepoint) {
            return &font->glyph_cache[i];
        }
    }
    return NULL;
}

/* Add glyph to cache and atlas */
static glyph_cache_entry_t* cache_glyph(ocfx_font_t *font, uint32_t codepoint) {
    /* Load glyph */
    if (FT_Load_Char(font->ft_face, codepoint, FT_LOAD_RENDER)) {
        return NULL;
    }

    FT_GlyphSlot slot = font->ft_face->glyph;

    /* Check if we need a new row in atlas */
    if (font->atlas_x + slot->bitmap.width > (unsigned)font->atlas_width) {
        font->atlas_x = 0;
        font->atlas_y += font->atlas_row_height;
        font->atlas_row_height = 0;
    }

    /* Check if atlas is full */
    if (font->atlas_y + slot->bitmap.rows > (unsigned)font->atlas_height) {
        fprintf(stderr, "OCFX: Font atlas full!\n");
        return NULL;
    }

    /* Upload glyph to atlas */
    glBindTexture(GL_TEXTURE_2D, font->texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0,
                    font->atlas_x, font->atlas_y,
                    slot->bitmap.width, slot->bitmap.rows,
                    GL_RED, GL_UNSIGNED_BYTE,
                    slot->bitmap.buffer);

    /* Grow cache if needed */
    if (font->glyph_cache_size >= font->glyph_cache_capacity) {
        size_t new_cap = font->glyph_cache_capacity * 2;
        if (new_cap == 0) new_cap = 128;

        glyph_cache_entry_t *new_cache = realloc(font->glyph_cache,
                                                   new_cap * sizeof(glyph_cache_entry_t));
        if (!new_cache) return NULL;

        font->glyph_cache = new_cache;
        font->glyph_cache_capacity = new_cap;
    }

    /* Add to cache */
    glyph_cache_entry_t *entry = &font->glyph_cache[font->glyph_cache_size++];
    entry->codepoint = codepoint;
    entry->atlas_x = (float)font->atlas_x / font->atlas_width;
    entry->atlas_y = (float)font->atlas_y / font->atlas_height;
    entry->atlas_width = (float)slot->bitmap.width / font->atlas_width;
    entry->atlas_height = (float)slot->bitmap.rows / font->atlas_height;
    entry->width = (float)slot->bitmap.width;
    entry->height = (float)slot->bitmap.rows;
    entry->bearing_x = (float)slot->bitmap_left;
    entry->bearing_y = (float)slot->bitmap_top;
    entry->advance = (float)(slot->advance.x >> 6);

    /* Update atlas position */
    font->atlas_x += slot->bitmap.width;
    if (slot->bitmap.rows > font->atlas_row_height) {
        font->atlas_row_height = slot->bitmap.rows;
    }

    return entry;
}

/* Get or cache glyph */
static glyph_cache_entry_t* get_glyph(ocfx_font_t *font, uint32_t codepoint) {
    glyph_cache_entry_t *entry = find_glyph(font, codepoint);
    if (!entry) {
        entry = cache_glyph(font, codepoint);
    }
    return entry;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

ocfx_font_t* ocfx_font_load(ocfx_renderer_t *renderer, const char *font_path, int size) {
    if (!renderer || !font_path || size <= 0) return NULL;

    ocfx_font_t *font = calloc(1, sizeof(ocfx_font_t));
    if (!font) return NULL;

    font->renderer = renderer;
    font->size = size;

    /* Initialize FreeType */
    if (FT_Init_FreeType(&font->ft_library)) {
        fprintf(stderr, "OCFX: Failed to initialize FreeType\n");
        free(font);
        return NULL;
    }

    /* Load font */
    if (FT_New_Face(font->ft_library, font_path, 0, &font->ft_face)) {
        fprintf(stderr, "OCFX: Failed to load font: %s\n", font_path);
        FT_Done_FreeType(font->ft_library);
        free(font);
        return NULL;
    }

    /* Set size */
    FT_Set_Pixel_Sizes(font->ft_face, 0, size);

    /* Get metrics */
    font->height = font->ft_face->size->metrics.height >> 6;
    font->advance = font->ft_face->size->metrics.max_advance >> 6;
    font->ascent = font->ft_face->size->metrics.ascender >> 6;
    font->descent = font->ft_face->size->metrics.descender >> 6;

    /* Create texture atlas */
    font->atlas_width = 2048;
    font->atlas_height = 2048;
    font->atlas_x = 0;
    font->atlas_y = 0;
    font->atlas_row_height = 0;

    glGenTextures(1, &font->texture);
    glBindTexture(GL_TEXTURE_2D, font->texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 font->atlas_width, font->atlas_height, 0,
                 GL_RED, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    /* Create shader program */
    font->shader_program = create_text_shader_program();
    if (!font->shader_program) {
        ocfx_font_destroy(font);
        return NULL;
    }

    /* Create VAO and VBO */
    glGenVertexArrays(1, &font->vao);
    glGenBuffers(1, &font->vbo);

    return font;
}

ocfx_font_t* ocfx_font_load_system(ocfx_renderer_t *renderer, const char *font_name, int size) {
    (void)font_name;  /* TODO: Search system font directories */

    /* Try common monospace fonts */
    const char *font_paths[] = {
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/liberation-mono/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        NULL
    };

    for (int i = 0; font_paths[i]; i++) {
        ocfx_font_t *font = ocfx_font_load(renderer, font_paths[i], size);
        if (font) return font;
    }

    return NULL;
}

void ocfx_font_destroy(ocfx_font_t *font) {
    if (!font) return;

    if (font->vbo) glDeleteBuffers(1, &font->vbo);
    if (font->vao) glDeleteVertexArrays(1, &font->vao);
    if (font->shader_program) glDeleteProgram(font->shader_program);
    if (font->texture) glDeleteTextures(1, &font->texture);
    if (font->glyph_cache) free(font->glyph_cache);
    if (font->ft_face) FT_Done_Face(font->ft_face);
    if (font->ft_library) FT_Done_FreeType(font->ft_library);

    free(font);
}

/* Font metrics */
int ocfx_font_get_height(ocfx_font_t *font) {
    return font ? font->height : 0;
}

int ocfx_font_get_advance(ocfx_font_t *font) {
    return font ? font->advance : 0;
}

int ocfx_font_get_ascent(ocfx_font_t *font) {
    return font ? font->ascent : 0;
}

int ocfx_font_get_descent(ocfx_font_t *font) {
    return font ? font->descent : 0;
}

/* Text measurement */
void ocfx_text_measure(ocfx_font_t *font, const char *text, float *width, float *height) {
    if (!font || !text) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    float w = 0;
    for (const char *p = text; *p; p++) {
        glyph_cache_entry_t *glyph = get_glyph(font, (uint32_t)*p);
        if (glyph) {
            w += glyph->advance;
        }
    }

    if (width) *width = w;
    if (height) *height = (float)font->height;
}

void ocfx_text_measure_n(ocfx_font_t *font, const char *text, size_t len,
                         float *width, float *height) {
    if (!font || !text) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }

    float w = 0;
    for (size_t i = 0; i < len && text[i]; i++) {
        glyph_cache_entry_t *glyph = get_glyph(font, (uint32_t)text[i]);
        if (glyph) {
            w += glyph->advance;
        }
    }

    if (width) *width = w;
    if (height) *height = (float)font->height;
}

/* Text rendering */
void ocfx_text_draw(ocfx_renderer_t *renderer, ocfx_font_t *font,
                    const char *text, float x, float y, ocfx_color_t color) {
    if (!renderer || !font || !text) return;

    /* Get viewport for shader uniform */
    int32_t vp_width, vp_height;
    ocfx_render_get_viewport(renderer, &vp_width, &vp_height);

    glUseProgram(font->shader_program);

    /* Set uniforms */
    GLint u_resolution = glGetUniformLocation(font->shader_program, "u_resolution");
    glUniform2f(u_resolution, (float)vp_width, (float)vp_height);

    GLint u_texture = glGetUniformLocation(font->shader_program, "u_texture");
    glUniform1i(u_texture, 0);

    /* Bind texture */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->texture);

    /* Render each character */
    /* Y coordinate is treated as TOP of text, convert to baseline
     * In screen space: (0,0) is top-left, Y increases downward
     * ascent is positive, represents pixels above baseline
     * So baseline = top + ascent */
    float pen_x = x;
    float pen_y = y + (float)font->ascent;

    for (const char *p = text; *p; p++) {
        glyph_cache_entry_t *glyph = get_glyph(font, (uint32_t)*p);
        if (!glyph) continue;

        float x0 = pen_x + glyph->bearing_x;
        float y0 = pen_y - glyph->bearing_y;
        float x1 = x0 + glyph->width;
        float y1 = y0 + glyph->height;

        float tx0 = glyph->atlas_x;
        float ty0 = glyph->atlas_y;
        float tx1 = tx0 + glyph->atlas_width;
        float ty1 = ty0 + glyph->atlas_height;

        /* Vertex data: pos(x,y) + texcoord(u,v) + color(r,g,b,a) */
        float vertices[] = {
            x0, y0, tx0, ty0, color.r, color.g, color.b, color.a,
            x1, y0, tx1, ty0, color.r, color.g, color.b, color.a,
            x0, y1, tx0, ty1, color.r, color.g, color.b, color.a,

            x1, y0, tx1, ty0, color.r, color.g, color.b, color.a,
            x1, y1, tx1, ty1, color.r, color.g, color.b, color.a,
            x0, y1, tx0, ty1, color.r, color.g, color.b, color.a,
        };

        glBindVertexArray(font->vao);
        glBindBuffer(GL_ARRAY_BUFFER, font->vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        pen_x += glyph->advance;
    }

    glBindVertexArray(0);
}

void ocfx_text_draw_n(ocfx_renderer_t *renderer, ocfx_font_t *font,
                      const char *text, size_t len, float x, float y, ocfx_color_t color) {
    if (!renderer || !font || !text) return;

    /* Simple implementation: copy string and null-terminate */
    char *temp = malloc(len + 1);
    if (!temp) return;

    memcpy(temp, text, len);
    temp[len] = '\0';

    ocfx_text_draw(renderer, font, temp, x, y, color);

    free(temp);
}

/* Advanced text rendering - stubs for now */
void ocfx_text_draw_aligned(ocfx_renderer_t *renderer, ocfx_font_t *font,
                            const char *text, ocfx_rect_t rect,
                            ocfx_text_align_t align, ocfx_text_baseline_t baseline,
                            ocfx_color_t color) {
    (void)rect;
    (void)align;
    (void)baseline;
    /* TODO: Implement alignment */
    ocfx_text_draw(renderer, font, text, rect.x, rect.y, color);
}

void ocfx_text_draw_wrapped(ocfx_renderer_t *renderer, ocfx_font_t *font,
                            const char *text, ocfx_rect_t rect, float line_spacing,
                            ocfx_color_t color) {
    (void)rect;
    (void)line_spacing;
    /* TODO: Implement word wrapping */
    ocfx_text_draw(renderer, font, text, rect.x, rect.y, color);
}

/* UTF-8 support - stubs */
bool ocfx_text_is_valid_utf8(const char *text, size_t len) {
    (void)text;
    (void)len;
    /* TODO: Implement UTF-8 validation */
    return true;
}

size_t ocfx_text_utf8_char_count(const char *text) {
    if (!text) return 0;
    /* Simple byte count for now */
    return strlen(text);
}
