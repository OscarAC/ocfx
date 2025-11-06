/* OCFX Example - Hello World
 * Simple demonstration of OCFX library usage
 */

#include <ocfx/ocfx.h>
#include <stdio.h>
#include <stdbool.h>

int main(void) {
    /* Initialize OCFX */
    ocfx_init();

    /* Create window */
    ocfx_window_config_t window_config = {
        .title = "OCFX - Hello World",
        .width = 800,
        .height = 600,
        .resizable = true,
        .decorated = true,
        .user_data = NULL,
    };

    ocfx_window_t *window = ocfx_window_create(&window_config);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        return 1;
    }

    /* Create renderer */
    ocfx_renderer_t *renderer = ocfx_renderer_create(window);
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer\n");
        ocfx_window_destroy(window);
        return 1;
    }

    /* Load font */
    ocfx_font_t *font = ocfx_font_load_system(renderer, "monospace", 24);
    if (!font) {
        fprintf(stderr, "Failed to load font\n");
        ocfx_renderer_destroy(renderer);
        ocfx_window_destroy(window);
        return 1;
    }

    printf("OCFX Hello World started!\n");
    printf("Press ESC to quit\n");

    /* Simple key callback */
    bool running = true;
    static bool *running_ptr = NULL;
    static ocfx_renderer_t *renderer_ptr = NULL;

    void key_callback(ocfx_window_t *win, const ocfx_key_event_t *event, void *data) {
        (void)win;
        (void)data;

        if (event->key == OCFX_KEY_ESCAPE) {
            if (running_ptr) {
                *running_ptr = false;
            }
        }
    }

    void resize_callback(ocfx_window_t *win, int32_t width, int32_t height, void *data) {
        (void)win;
        (void)data;

        /* Update renderer viewport when window is resized */
        if (renderer_ptr) {
            ocfx_render_set_viewport(renderer_ptr, width, height);
        }

        printf("Window resized to %dx%d\n", width, height);
    }

    running_ptr = &running;
    renderer_ptr = renderer;
    ocfx_input_set_key_callback(window, key_callback);
    ocfx_window_set_resize_callback(window, resize_callback);

    /* Main loop */
    while (running && !ocfx_window_should_close(window)) {
        /* Handle events */
        if (ocfx_window_dispatch(window) < 0) {
            break;
        }

        /* Render frame */
        ocfx_render_begin(renderer, OCFX_COLOR_RGB(0.1f, 0.1f, 0.15f));

        /* Draw a rectangle */
        ocfx_draw_rect_filled(renderer,
                              OCFX_RECT(50, 50, 200, 150),
                              OCFX_COLOR_RGB(0.2f, 0.4f, 0.8f));

        /* Draw rectangle outline */
        ocfx_draw_rect_outline(renderer,
                               OCFX_RECT(300, 50, 200, 150),
                               OCFX_COLOR_RGB(0.8f, 0.4f, 0.2f),
                               2.0f);

        /* Draw a circle */
        ocfx_draw_circle_filled(renderer,
                                OCFX_POINT(150, 350),
                                50.0f,
                                OCFX_COLOR_RGB(0.4f, 0.8f, 0.2f));

        /* Draw text */
        ocfx_text_draw(renderer, font,
                      "Hello, OCFX!",
                      50, 450,
                      OCFX_COLOR_WHITE);

        ocfx_text_draw(renderer, font,
                      "Press ESC to quit",
                      50, 490,
                      OCFX_COLOR_RGB(0.7f, 0.7f, 0.7f));

        ocfx_render_end(renderer);
        ocfx_render_present(renderer);
    }

    printf("Shutting down...\n");

    /* Cleanup */
    ocfx_font_destroy(font);
    ocfx_renderer_destroy(renderer);
    ocfx_window_destroy(window);
    ocfx_cleanup();

    printf("Done!\n");
    return 0;
}
