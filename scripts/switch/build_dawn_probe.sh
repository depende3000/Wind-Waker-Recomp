#!/usr/bin/env bash
# Build the pinned Dawn OpenGLES offscreen probe as a local Switch NRO.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
devkitpro=${DEVKITPRO:-/opt/devkitpro}
image=${DEVKITPRO_DAWN_BUILD_IMAGE:-localhost/wwrecomp-switch-dawn-build:2026-10-02}
build_dir="$root/build/switch-dawn-probe"
jobs=${SWITCH_BUILD_JOBS:-4}

if [[ ${1:-} == clean ]]; then
    rm -rf "$build_dir"
    exit 0
fi

build_nro() {
    cmake -S "$root/switch/dawn" -B "$build_dir" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$devkitpro/cmake/Switch.cmake" \
        -DDEVKITPRO_ROOT="$devkitpro" \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "$build_dir" --target BlueWakeDawnOffscreenProbe --parallel "$jobs"
}

if [[ -f "$devkitpro/cmake/Switch.cmake" ]] &&
   command -v cmake >/dev/null 2>&1 &&
    command -v ninja >/dev/null 2>&1; then
    build_nro
else
    if ! command -v podman >/dev/null 2>&1; then
        echo "Dawn Switch build requires CMake, Ninja, devkitPro, or Podman." >&2
        exit 1
    fi
    if ! podman image exists "$image"; then
        podman build --tag "$image" --file "$root/scripts/switch/Containerfile.dawn" "$root"
    fi
    podman run --rm --userns=keep-id \
        -v "$root:/work:Z" \
        -w /work \
        -e SWITCH_BUILD_JOBS="$jobs" \
        "$image" \
        bash -lc 'cmake -S /work/switch/dawn -B /work/build/switch-dawn-probe -G Ninja \
                    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake \
                    -DDEVKITPRO_ROOT=/opt/devkitpro \
          -DCMAKE_BUILD_TYPE=Release && \
          cmake --build /work/build/switch-dawn-probe --target BlueWakeDawnOffscreenProbe \
            --parallel "${SWITCH_BUILD_JOBS:-4}"'
fi

nro="$build_dir/BlueWakeDawnOffscreenProbe.nro"
if [[ ! -s "$nro" ]] || [[ $(od -An -tc -j 16 -N4 "$nro" | tr -d ' \n') != NRO0 ]]; then
    echo "Dawn Switch build did not produce a valid NRO: $nro" >&2
    exit 1
fi
sha256sum "$nro"
printf 'Built %s (%s bytes)\n' "$nro" "$(stat -c '%s' "$nro")"