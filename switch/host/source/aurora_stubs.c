// The headless Switch build has no Aurora: these stand-ins report that it
// cannot start, so main.c takes its headless path (BLUEWAKE_RENDERER=headless
// is also set by switch_main.c). Not built with BLUEWAKE_SWITCH_AURORA.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "gxruntime/aurora_backend.h"
#include <aurora/gfx.h>

bool dol_aurora_initialize(int argc, char** argv, const AuroraBackendConfig* config) {
    (void)argc;
    (void)argv;
    (void)config;
    return false;
}
void dol_aurora_shutdown(void) {}
void dol_aurora_set_fast_forward(bool on) { (void)on; }
void dol_aurora_frame_timing(DolAuroraFrameTiming* out) {
    if (out != NULL)
        memset(out, 0, sizeof *out);
}
void aurora_backend_service_present(void) {}
size_t dol_aurora_gx_save_state(void** out) {
    if (out != NULL)
        *out = NULL;
    return 0;
}
bool dol_aurora_gx_load_state(const void* data, size_t size) {
    (void)data;
    (void)size;
    return false;
}
void dol_aurora_gx_drain(void) {}

void aurora_request_framebuffer_readback(void) {}
bool aurora_take_framebuffer_readback(const uint8_t** rgba, uint32_t* width, uint32_t* height) {
    (void)rgba;
    (void)width;
    (void)height;
    return false;
}
bool aurora_peek_z(uint16_t x, uint16_t y, uint32_t* z) {
    (void)x;
    (void)y;
    (void)z;
    return false;
}
