#!/usr/bin/env bash
# Build the Switch host NRO from the generated game source.
#
#   scripts/switch/build_host.sh [--aurora] [COMPOSITE_SRC]
#
# COMPOSITE_SRC defaults to build/device/composite-src, which
#   scripts/builder/build.sh DISC.iso --source-only
# generates from the player's own disc. --aurora builds with the Aurora
# renderer (BLUEWAKE_SWITCH_AURORA); the default is the headless build.
#
# 1. The composite compiles here, natively, with clang for aarch64-none-elf
#    against devkitA64's newlib headers (switch/composite/clang-switch.cmake):
#    about 1.7x faster than devkitA64's GCC at half the memory. Needs clang
#    (Xcode's on macOS), CMake and Ninja.
# 2. In the pinned devkitPro container: its objects are linked into one
#    relocatable object with every symbol prefixed bwc_, then the host NRO is
#    built and linked against it.
#
# Outputs stay under build/; the NRO contains the translated game code and is
# for the player's own console only. SWITCH_BUILD_JOBS sets parallel jobs
# (default: all cores for the composite, 4 in the container).
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
aurora=OFF
if [[ ${1:-} == --aurora ]]; then
    aurora=ON
    shift
fi
composite_src=${1:-build/device/composite-src}
image=${DEVKITPRO_DAWN_BUILD_IMAGE:-localhost/wwrecomp-switch-dawn-build:2026-10-02}
toolchain_image=docker.io/devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282
cores=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
composite_jobs=${SWITCH_BUILD_JOBS:-$cores}
container_jobs=${SWITCH_BUILD_JOBS:-4}
source "$root/scripts/switch/container.sh"

case $composite_src in
    /*) ;;
    *) composite_src="$root/$composite_src" ;;
esac
if [[ ! -f $composite_src/generated_composite.h ]]; then
    echo "build_host: no generated composite at $composite_src" >&2
    echo "Generate it with: scripts/builder/build.sh DISC.iso --source-only" >&2
    exit 1
fi

recompcore="$root/ref/recompcore"
if [[ ! -d $recompcore/.git && ! -f $recompcore/.git ]]; then
    echo "build_host: run scripts/bootstrap.sh first" >&2
    exit 1
fi
# The donor DSP needs two of RecompCore's submodules that bootstrap.sh skips.
if [[ ! -f $recompcore/Externals/fmt/fmt/src/format.cc ||
      ! -f $recompcore/Externals/xxhash/xxHash/xxhash.c ]]; then
    git -C "$recompcore" submodule update --init --depth 1 \
        Externals/fmt/fmt Externals/xxhash/xxHash
fi

engine=$(container_engine)
if [[ -z $engine ]]; then
    echo "build_host: Podman or Docker is required" >&2
    exit 1
fi
if ! container_image_exists "$engine" "$image"; then
    "$engine" build --tag "$image" --file "$root/scripts/switch/Containerfile.dawn" \
        "$root/scripts/switch"
fi

# devkitA64's newlib headers, from the pinned toolchain image, for clang.
newlib="$root/build/switch-tools/newlib-include"
if [[ ! -f $newlib/stdlib.h ]]; then
    mkdir -p "$root/build/switch-tools"
    container=$("$engine" create "$toolchain_image")
    "$engine" cp "$container:/opt/devkitpro/devkitA64/aarch64-none-elf/include" "$newlib"
    "$engine" rm "$container" >/dev/null
fi

# 1. The composite's objects, natively.
composite_build="$root/build/switch-composite-clang"
cmake -S "$root/cmake/composite" -B "$composite_build" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$root/switch/composite/clang-switch.cmake" \
    -DSWITCH_NEWLIB_INCLUDE="$newlib" -DCMAKE_BUILD_TYPE=Release \
    -DCOMPOSITE_STATIC=ON -DCOMPOSITE_STATIC_LINK=OFF -DCOMPOSITE_DIR="$composite_src" \
    -DGXRUNTIME_DIR="$recompcore/GXRuntime" \
    -DABI_DIR="$recompcore/Source/Core/Core/PowerPC/StaticRecomp" >/dev/null
cmake --build "$composite_build" --parallel "$composite_jobs"

# 2. Link and prefix them, then the host, in the container. The object list
# holds this machine's absolute paths, so the repository is also mounted there.
host_build=build/switch-host
[[ $aurora == ON ]] && host_build=build/switch-host-aurora
container_run "$engine" "$root" -v "$root:$root" -e JOBS="$container_jobs" \
    -e ROOT="$root" -e AURORA="$aurora" -e HOST_BUILD="$host_build" "$image" bash -lc '
set -euo pipefail
export PATH=/opt/devkitpro/devkitA64/bin:$PATH
composite=$ROOT/build/switch-composite-clang
aarch64-none-elf-ld -r -o "$composite/gGZLE01_recomp.combined.o" "@$composite/gGZLE01_recomp.objects.rsp"
cmake -DNM=aarch64-none-elf-nm -DOBJCOPY=aarch64-none-elf-objcopy -DPREFIX=bwc_ \
    -DINPUT="$composite/gGZLE01_recomp.combined.o" -DOUTPUT="$composite/gGZLE01_recomp.o" \
    -P /work/cmake/composite/prefix_symbols.cmake
cmake -S /work/switch/host -B "/work/$HOST_BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release \
    -DBLUEWAKE_SWITCH_AURORA="$AURORA" -DBLUEWAKE_COMPOSITE_OBJECT="$composite/gGZLE01_recomp.o" >/dev/null
cmake --build "/work/$HOST_BUILD" --parallel "$JOBS"
'

nro="$root/$host_build/BlueWakeSwitch.nro"
if [[ ! -s $nro ]] || [[ $(od -An -tc -j 16 -N4 "$nro" | tr -d ' \n') != NRO0 ]]; then
    echo "build_host: no valid NRO at $nro" >&2
    exit 1
fi
sha256sum "$nro"
printf 'Built %s (%s bytes)\n' "$nro" "$(wc -c <"$nro" | tr -d ' ')"
