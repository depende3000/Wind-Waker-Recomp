#!/usr/bin/env bash
# Build the SDL2 + switch-mesa GLES feasibility probe using the same pinned
# devkitPro toolchain as the libnx bootstrap probe.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
exec bash "$root/scripts/switch/build_probe.sh" \
    TARGET=BlueWakeGlesProbe \
    SOURCES=source/gles \
    BUILD=../build/switch-gles-probe \
    APP_TITLE="Wind Waker Recomp GLES Probe" \
    LIBS="-lSDL2 -lEGL -lGLESv2 -lstdc++ -lglapi -ldrm_nouveau -lnx -lpthread -lm" \
    "$@"
