# OCFX - Wayland Graphics Library

A minimal, high-performance graphics framework for Wayland compositors.

## Overview

OCFX provides a simple, clean API for building GPU-accelerated applications on Wayland. It handles:

- **Wayland Protocol** - Window creation, events, input
- **OpenGL ES 3.0** - GPU-accelerated 2D rendering
- **Text Rendering** - FreeType-based font rendering with texture atlas
- **Input Handling** - Keyboard and mouse events with XKB support

## Design Philosophy

- **Minimal** - Small codebase, focused API
- **Fast** - GPU acceleration, efficient rendering
- **Clean** - Simple, well-documented interfaces
- **Modular** - Use only what you need
- **Suckless** - Compile-time configuration, static linking

## Architecture

```
OCFX Library (libocfx.a)
├─ Wayland Client (ocfx/wayland.h)
├─ OpenGL Renderer (ocfx/render.h)
├─ Text Rendering (ocfx/text.h)
├─ Input Handling (ocfx/input.h)
└─ Common Types (ocfx/types.h)
```

## Building

```bash
make              # Build static library
make examples     # Build example programs
make install      # Install to /usr/local
make clean        # Clean build artifacts
```

## Usage

```c
#include <ocfx/ocfx.h>

int main() {
    /* Initialize OCFX */
    ocfx_init();

    /* Create window */
    ocfx_window_config_t config = {
        .title = "Hello OCFX",
        .width = 800,
        .height = 600,
    };
    ocfx_window_t *window = ocfx_window_create(&config);

    /* Create renderer */
    ocfx_renderer_t *renderer = ocfx_renderer_create(window);

    /* Load font */
    ocfx_font_t *font = ocfx_font_load_system(renderer, "monospace", 14);

    /* Main loop */
    while (!ocfx_window_should_close(window)) {
        /* Handle events */
        ocfx_window_dispatch(window);

        /* Render */
        ocfx_render_begin(renderer, OCFX_COLOR_BLACK);

        ocfx_text_draw(renderer, font, "Hello, OCFX!",
                      10, 10, OCFX_COLOR_WHITE);

        ocfx_render_end(renderer);
        ocfx_render_present(renderer);
    }

    /* Cleanup */
    ocfx_font_destroy(font);
    ocfx_renderer_destroy(renderer);
    ocfx_window_destroy(window);
    ocfx_cleanup();

    return 0;
}
```

## Dependencies

- wayland-client
- wayland-egl
- xkbcommon
- EGL
- GLESv2
- freetype2

## License

MIT License.

## Version

Current: 0.1.0 (in development)
