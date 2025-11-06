/* OCFX - Optimal Computing Framework X
 * Main header - include this to get all OCFX functionality
 */

#ifndef OCFX_H
#define OCFX_H

#define OCFX_VERSION_MAJOR 0
#define OCFX_VERSION_MINOR 1
#define OCFX_VERSION_PATCH 0

/* Core modules */
#include "ocfx/types.h"
#include "ocfx/wayland.h"
#include "ocfx/render.h"
#include "ocfx/text.h"
#include "ocfx/input.h"

/* Utility functions */
const char* ocfx_version_string(void);
int ocfx_init(void);
void ocfx_cleanup(void);

#endif /* OCFX_H */
