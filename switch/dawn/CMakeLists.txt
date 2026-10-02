cmake_minimum_required(VERSION 3.24)
project(BlueWakeDawnOffscreenProbe LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/dawn.cmake")

add_executable(dawn_offscreen_probe_elf
    source/dawn_offscreen_probe.cpp
    ../source/common/usb_log.c
)
target_include_directories(dawn_offscreen_probe_elf SYSTEM PRIVATE
    "${DEVKITPRO_ROOT}/portlibs/switch/include"
)
target_include_directories(dawn_offscreen_probe_elf PRIVATE ../source/common)
target_link_libraries(dawn_offscreen_probe_elf PRIVATE
    dawn::webgpu_dawn
    EGL
    drm_nouveau
    GLESv2
    glapi
    nx
)
# -g keeps the probe's own symbols in the ELF for crash-report addresses;
# elf2nro copies only loadable segments, so the NRO is unaffected.
target_compile_options(dawn_offscreen_probe_elf PRIVATE -Wall -Wextra -Wpedantic -g)

nx_create_nro(
    BlueWakeDawnOffscreenProbe
    TARGET dawn_offscreen_probe_elf
    OUTPUT "${CMAKE_BINARY_DIR}/BlueWakeDawnOffscreenProbe.nro"
    NOICON
    NONACP
)