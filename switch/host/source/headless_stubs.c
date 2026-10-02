// The Switch headless milestone builds the host without Aurora and without
// the SDL-based extras (mouse camera, jump button, sprint, options menu).
// These definitions stand in for them: the extras stay inert, and Aurora
// reports that it cannot start, so main.c takes its headless path
// (BLUEWAKE_RENDERER=headless is also set by switch_main.c).
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "core/cpu.h"
#include "gxruntime/aurora_backend.h"
#include "gxruntime/si.h"
#include <aurora/gfx.h>

#include "jump_button.h"
#include "mouse_camera.h"
#include "settings_menu.h"
#include "sprint.h"

bool bluewake_jump_button_armed = false;
void bluewake_jump_button_attach(CPUState* cpu) { (void)cpu; }
void bluewake_jump_button_event(const void* sdl_event) { (void)sdl_event; }
void bluewake_jump_button_retrace(void) {}
void bluewake_jump_button_reload(void) {}
bool bluewake_jump_button_enter(CPUState* cpu, u32 address) {
    (void)cpu;
    (void)address;
    return false;
}

void bluewake_mouse_camera_install(void) {}
void bluewake_mouse_camera_attach(CPUState* cpu) { (void)cpu; }
void bluewake_mouse_camera_retrace(void) {}
void bluewake_mouse_camera_hook(CPUState* cpu, u32 address) {
    (void)cpu;
    (void)address;
}
void bluewake_mouse_camera_pad(DolPadState* pad) { (void)pad; }
bool bluewake_mouse_camera_scripted(void) { return false; }
bool bluewake_mouse_camera_captured(void) { return false; }
void bluewake_mouse_camera_release(void) {}
void bluewake_mouse_camera_reload(void) {}

void bluewake_sprint_attach(CPUState* cpu) { (void)cpu; }
void bluewake_sprint_retrace(void) {}
void bluewake_sprint_reload(void) {}

void bluewake_settings_load(void) {}
void bluewake_settings_menu_install(void) {}
bool bluewake_settings_menu_event(const void* sdl_event) {
    (void)sdl_event;
    return false;
}

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
