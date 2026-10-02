// The Switch build leaves out the host's SDL-based extras (mouse camera, jump
// button, sprint, options menu); these inert definitions stand in for them.
#include <stdbool.h>
#include <stddef.h>

#include "core/cpu.h"
#include "gxruntime/si.h"

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
