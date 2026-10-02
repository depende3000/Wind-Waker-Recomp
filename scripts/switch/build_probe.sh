#!/usr/bin/env bash
# Build the Switch bootstrap probe with either a local devkitPro install or a
# pinned official toolchain container. The container path is rootless.
set -euo pipefail

root=$(cd "$(dirname "$0")/../.." && pwd)
devkitpro=${DEVKITPRO:-/opt/devkitpro}
image=${DEVKITPRO_SWITCH_IMAGE:-docker.io/devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282}

if [[ -f "$devkitpro/libnx/switch_rules" &&
      -x "$devkitpro/devkitA64/bin/aarch64-none-elf-gcc" ]] &&
      command -v make >/dev/null 2>&1; then
    DEVKITPRO="$devkitpro" make -C "$root/switch" "$@"
    exit $?
fi

if ! command -v podman >/dev/null 2>&1; then
    echo "switch build: no local devkitPro Switch toolchain and Podman is unavailable" >&2
    echo "Install devkitPro switch-dev or install Podman and rerun this script." >&2
    exit 1
fi

podman run --rm --userns=keep-id \
    -v "$root:/work:Z" \
    -w /work \
    "$image" \
    make -C switch "$@"
