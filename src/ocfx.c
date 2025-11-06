/* OCFX - Main Implementation
 */

#include "ocfx/ocfx.h"
#include <stdio.h>

const char* ocfx_version_string(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             OCFX_VERSION_MAJOR, OCFX_VERSION_MINOR, OCFX_VERSION_PATCH);
    return version;
}

int ocfx_init(void) {
    /* Global initialization if needed */
    return OCFX_OK;
}

void ocfx_cleanup(void) {
    /* Global cleanup if needed */
}
