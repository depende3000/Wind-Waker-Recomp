#!/usr/bin/env bash
# Build the Switch host NRO (headless milestone) from the generated game
# source: the composite as one static object (cmake/composite with
# COMPOSITE_STATIC), then switch/host linked against it.
#
#   scripts/switch/build_host.sh [COMPOSITE_SRC]
#
# COMPOSITE_SRC defaults to build/device/composite-src, which
#   scripts/builder/build.sh DISC.iso --source-only
# generates from the player's own disc. Outputs stay under build/; the NRO
# contains the translated game code and is for the player's own console only.
# SWITCH_BUILD_JOBS sets parallel jobs (default 6; the largest generated
# chunks need about 3 GB of memory each to compile).
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
composite_src=${1:-build/device/composite-src}
image=${DEVKITPRO_DAWN_BUILD_IMAGE:-localhost/wwrecomp-switch-dawn-build:2026-10-02}
jobs=${SWITCH_BUILD_JOBS:-6}
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
if [[ $composite_src != "$root"/* ]]; then
    echo "build_host: COMPOSITE_SRC must be inside the repository (it is mounted at /work)" >&2
    exit 1
fi
container_src=/work/${composite_src#"$root"/}

engine=$(container_engine)
if [[ -z $engine ]]; then
    echo "build_host: Podman or Docker is required" >&2
    exit 1
fi
if ! container_image_exists "$engine" "$image"; then
    "$engine" build --tag "$image" --file "$root/scripts/switch/Containerfile.dawn" \
        "$root/scripts/switch"
fi

container_run "$engine" "$root" -e JOBS="$jobs" -e COMPOSITE_SRC="$container_src" "$image" \
    bash -lc '
set -euo pipefail
toolchain="-DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release"
cmake -S /work/cmake/composite -B /work/build/switch-composite -G Ninja $toolchain \
    -DCOMPOSITE_STATIC=ON -DCOMPOSITE_DIR="$COMPOSITE_SRC" \
    -DGXRUNTIME_DIR=/work/ref/recompcore/GXRuntime \
    -DABI_DIR=/work/ref/recompcore/Source/Core/Core/PowerPC/StaticRecomp
cmake --build /work/build/switch-composite --parallel "$JOBS"
cmake -S /work/switch/host -B /work/build/switch-host -G Ninja $toolchain \
    -DBLUEWAKE_COMPOSITE_OBJECT=/work/build/switch-composite/gGZLE01_recomp.o
cmake --build /work/build/switch-host --parallel "$JOBS"
'

nro="$root/build/switch-host/BlueWakeSwitch.nro"
if [[ ! -s $nro ]] || [[ $(od -An -tc -j 16 -N4 "$nro" | tr -d ' \n') != NRO0 ]]; then
    echo "build_host: no valid NRO at $nro" >&2
    exit 1
fi
sha256sum "$nro"
printf 'Built %s (%s bytes)\n' "$nro" "$(wc -c <"$nro" | tr -d ' ')"
