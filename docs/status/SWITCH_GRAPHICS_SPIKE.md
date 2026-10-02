# Switch graphics feasibility spike — preliminary evidence

**Date:** 2026-10-02  
**Status:** Physical Switch logs confirm bootstrap SD/log operation and SDL2/Mesa GLES 3.2 rendering with successful framebuffer copy. Dawn's initial adapter enumeration was blocked first by missing robust-context support; the expanded EGL log also shows no EGL fence/reusable/native-fence sync. A diagnostic-only `glFinish` queue fallback is now cross-built, but has not been rerun. Gate A remains **open**: Dawn context/render/readback, Aurora/GX, game-renderer EFB-copy evidence, and timed stability are missing.

## Question

Can the existing Aurora GX renderer be used on Switch through a public, reproducible homebrew graphics path, so a later NRO can display the real recompiled game? A text console, framebuffer clear, or standalone GLES triangle is only platform plumbing; it does not satisfy this gate.

## Local build environment

The official `devkitpro/devkita64` image is pulled rootlessly through Podman and pinned by digest in [build_probe.sh](../../scripts/switch/build_probe.sh):

- Image: `docker.io/devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282`
- devkitA64 GCC 15.2.0
- libnx 4.12.0-1
- deko3d 0.5.0-1
- switch-mesa 20.1.0-5
- switch-sdl2 2.28.5-4

The image provides SDL2, EGL, GLESv2, Mesa, and deko3d for Switch homebrew. It has no SDL3 or Dawn installation. Host CMake 4.4.3 and Ninja 1.13.2 are installed through Linuxbrew. The Aurora cross-configure and Dawn NRO use a local derived container image with Debian CMake 3.31.6, Ninja 1.11.1, pkg-config, and Python 3.11.2. Its pinned recipe is [Containerfile.dawn](../../scripts/switch/Containerfile.dawn); generated images and build products are not committed.

Two local artifacts build:

- `build/switch-probe/BlueWakeSwitchProbe.nro`: 209 KiB, NRO magic verified. It exercises the libnx diagnostic text console and checks the automatic SD device/log. Corrected build SHA-256 `39cc96396a9465820ce582202b6a356f59a5411fe57dd61e32b4e48d15e23cc4`.
- `build/switch-gles-probe/BlueWakeGlesProbe.nro`: 5.8 MiB, NRO magic verified. Current build SHA-256 `23ccf3cb4caf93b6978e6000efff19c0e9ea4453724a27e084ad444c2d47f729`. It creates an SDL2/Mesa GLES2 context, compiles an in-memory textured quad with alpha blending and depth testing, and checks a color framebuffer copy. Its log reports GL version/renderer, drawable/depth size, swap interval, 5-second FPS windows, runtime, and copy result at `sdmc:/switch/wind-waker-recomp/gles-probe.log`.
- Initial Dawn NRO: 11,489,280 bytes, `NRO0` header verified at offset `0x10`, SHA-256 `bee5895a37102f4f8b2638fa0a66dcafc6df7a8f6526aefb584faf706ee4ee83`. The Switch log reported `EGL_EXT_create_context_robustness is required`, then no OpenGLES adapter; no Dawn commands ran.
- Revised Dawn diagnostic NRO: 11,489,280 bytes, SHA-256 `355a1c98acc147c452773f7c1b296326ab8c246d062b502942bb68df3f28df90`. It logs EGL capabilities, disables robust buffer access only if robust contexts are unavailable, permits native-fence sync, and uses `glFinish` to complete queue serials only when no EGL sync extension exists. This is slow and diagnostic-only. **Build verified, physical rerun pending.** See [the probe notes](../../switch/dawn/README.md).

The user-provided bootstrap log reports `sd_available=1`, `log_ready=1`, and a normal stop, confirming SD detection, log creation, and clean exit. The GLES logs report `GL_VERSION=OpenGL ES 3.2 Mesa 20.1.0-rc3`, `GL_RENDERER=NV120`, a 1280×720 drawable with 16 depth bits, and `framebuffer color-copy passed (0x0000)`. Runs ended after 4.034 seconds and 2.351 seconds; the requested 10-minute soak is not established. The previous `fsdevMountSdmc()` failure `0x00000559` was caused by redundantly mounting `sdmc`, which libnx automatically mounts for an NRO. Both probes now query `fsdevGetDeviceFileSystem("sdmc")` and do not unmount it. The bootstrap SHA-256 is `39cc96396a9465820ce582202b6a356f59a5411fe57dd61e32b4e48d15e23cc4`; the GLES SHA-256 is `23ccf3cb4caf93b6978e6000efff19c0e9ea4453724a27e084ad444c2d47f729`. None of these probes links DolRecomp output or Aurora.

### Bootstrap SD diagnostic

The bootstrap build displayed `fsdevMountSdmc()` failure `0x00000559`. Per libnx's result macros, this decodes to `Module_Libnx` (345) and `LibnxError_OutOfMemory` (description 2). libnx's `fsdevMountSdmc()` maps failure to register the `sdmc` device to this code; its normal NRO example documents that `sdmc` is already mounted automatically. The bootstrap was incorrectly trying to mount it a second time. Both probes now query `fsdevGetDeviceFileSystem("sdmc")`, use the automatic mount, and do not unmount it. The user confirms the corrected bootstrap detects the SD device. Log creation and clean **+** exit remain to be confirmed.

## Renderer-stack findings

The source pins matter. This project pins RecompCore at `8ab24daee9c641634fda5cac30389ad4b2cfda5e`, which vendors its Aurora tree. That Aurora CMake setup supports provider selection for Dawn and SDL3, but its GX target links Dawn's WebGPU target and its main target links SDL3. It has no known libnx/deko3d provider in the inspected configuration. See the [pinned RecompCore Aurora CMake file](https://github.com/elliotttate/RecompCore/blob/8ab24daee9c641634fda5cac30389ad4b2cfda5e/GXRuntime/graphics/aurora/CMakeLists.txt), [Aurora GX target](https://github.com/encounter/aurora/blob/1d10fa1bc502910a6336fdac32f31cd0ac39710d/cmake/aurora_gx.cmake), and [Aurora main target](https://github.com/encounter/aurora/blob/1d10fa1bc502910a6336fdac32f31cd0ac39710d/cmake/aurora_main.cmake).

The pinned public SDL3 release documents Nintendo Switch support as a **separate repository requiring the appropriate Nintendo NDA**. That is not an available public homebrew SDL3 backend for this project. The standard devkitPro image supplies SDL2, not SDL3. See [SDL 3.4.10 Switch platform notes](https://github.com/libsdl-org/SDL/blob/release-3.4.10/docs/README-switch.md).

Dawn's pinned support notes list Vulkan 1.1 support on selected operating systems, not Horizon/libnx; its OpenGL path is described as work in progress, targeting GLES 3.1 through EGL. The standalone OpenGLES backend and an offscreen test NRO now cross-compile against the Switch SDK and static Mesa EGL/GLES libraries. This establishes build feasibility only: no console run has confirmed EGL initialization, adapter/device creation, pbuffer rendering, or Dawn's required GX feature set. See [Dawn's pinned platform support](https://github.com/encounter/dawn/blob/v20260618.032059/docs/support.md).

The exact pinned Aurora source confirms an additional integration seam: its [Dawn surface binding](https://github.com/elliotttate/RecompCore/blob/8ab24daee9c641634fda5cac30389ad4b2cfda5e/GXRuntime/graphics/aurora/lib/dawn/BackendBinding.cpp) handles Cocoa, Android, Windows, Linux Wayland and Xlib descriptors, but has no Switch case. Pinned Dawn's surface descriptor and EGL swapchain likewise handle Android, Metal, Windows and Xlib/Wayland—not libnx `NWindow`/EGL native windows. A Switch Dawn surface therefore needs an explicit Dawn surface type/backend integration; the existing SDL2/GLES context alone cannot be passed directly to Aurora.

A CMake configure was attempted against the pinned Aurora tree with the official devkitPro Switch toolchain and vendored Dawn/SDL3. It stopped while configuring upstream SDL3 with `Threads are needed by many SDL subsystems and may not be disabled`: SDL's platform detector does not identify the toolchain's `CMAKE_SYSTEM_NAME=NintendoSwitch`, so it selects no Switch video/thread platform. Forcing `SDL_PTHREADS=ON` did not help because SDL only invokes its thread backend checker inside recognized OS branches. This is separate from the NDA-only SDL3 port note and a direct source/configuration blocker to building the existing Aurora target on this toolchain.

The source audit verified two distinct Dawn presentation gaps. Aurora's SDL3-to-Dawn binding does not select or describe a Switch window. Dawn's core `Surface` type has no Horizon/libnx/NWindow descriptor, and its GLES `SwapChainEGL` only creates EGL window surfaces for Android, Metal, Win32, and Xlib types. Therefore adding `DAWN_ENABLE_OPENGLES=ON` alone cannot make Aurora present to the libnx SDL2/Mesa window; a new surface/window integration is required in addition to a usable SDL3 platform layer. The separate offscreen NRO avoids `Surface` and copies pixels through libnx framebuffer; it is explicitly not a Dawn window-surface proof.

## Standalone Dawn offscreen build

The [probe source and build notes](../../switch/dawn/README.md) use the exact Dawn archive pinned by Aurora's CMake (`266c1cf8de969a364afa4fa49311631fc99a881e`, SHA-256 `03ca9de39e1b534c9a443ede66ce8fcf61521edfa7d526f9356972241cbd957d`). The rootless wrapper is [build_dawn_probe.sh](../../scripts/switch/build_dawn_probe.sh); its CMake target uses devkitPro's NRO packaging helper and static `EGL`, `drm_nouveau`, `GLESv2`, and `glapi` libraries. The build needs small Horizon/newlib compatibility patches to the fetched Dawn, Abseil, and Tint sources; these live under [switch/dawn/patches](../../switch/dawn/patches) and apply only to the ignored build-tree copies.

The first NRO asked Dawn to initialize default-display EGL via `eglGetProcAddress`; Dawn adapter discovery rejected the display because `EGL_EXT_create_context_robustness` was unavailable. The next run logged EGL 1.4, Mesa Project, `robustness_ext_listed=no`, `robustness_available=no`, `fence_sync=no`, `reusable_sync=no`, `native_fence=no`, `surfaceless=yes`, and `pbuffer_config=unavailable`; the extension string contains only config attributes, create-context, get-all-proc-addresses, and surfaceless-context. Dawn then rejected adapter discovery for lacking `EGL_KHR_fence_sync` or `EGL_KHR_reusable_sync`. No adapter/device or render/readback was produced. The latest diagnostic build permits adapter discovery with no EGL sync extensions and makes `QueueGL` drain submissions using `glFinish` before advancing each serial. It also disables robust buffer access only for the fixed test shader if robust context creation is absent. These accommodations are **not safe for production/untrusted shader execution**; the fence fallback is synchronous, slow, and cannot export shared fences. `EGL_KHR_surfaceless_context` is advertised despite no pbuffer config, but Dawn's EGL config/context creation still needs runtime confirmation. The revised NRO has not yet been run. Even a pass does not exercise Aurora/GX, game assets, Dawn `Surface`, or NWindow presentation.

## Assessment

There is a **candidate public low-level path**: SDL2 + switch-mesa EGL/GLES, with a separate Dawn GLES offscreen proof now cross-built. The user has confirmed a visible draw through SDL2/Mesa on hardware. The Dawn NRO's runtime remains untested; even if it passes, it would not make the existing Aurora renderer portable as-is. Aurora expects SDL3 and Dawn, and Horizon surface interop is still absent.

A direct deko3d backend is technically available in the toolchain but would be a distinct renderer implementation. Aurora produces shader variants from runtime GX state; deko3d's toolchain uses precompiled shader programs, so integrating it would need an explicit shader-generation/compilation design. Do not begin that backend without a separate scope and go/no-go review.

**Decision:** the SDL2/Mesa path has cleared its first visible-draw substep, and the pinned Dawn GLES stack now cross-compiles into a standalone offscreen NRO. Do not start full host/composite integration. Physically run the Dawn NRO and inspect its adapter/EGL logs, pixel checks, framebuffer output, and stability; also collect the corrected GLES log for GL identity, copy/depth results, and the 10-minute soak. If Dawn offscreen fails, diagnose the EGL/feature gap before any surface work. If it passes, separately scope SDL3/Horizon integration plus a Dawn Switch surface or another approved presentation path. Gate A remains closed until Aurora-generated GX work, texture sampling, blend/depth, EFB copy, and the intended application presentation have passed on physical hardware.

## Next checks

1. The bootstrap SD-device check is confirmed by the user; if practical, still verify `boot-probe.log` creation and clean **+** exit.
2. Retest the corrected `build/switch-gles-probe/BlueWakeGlesProbe.nro`; capture `GL_VERSION`, `GL_RENDERER`, framebuffer-copy/depth results, and a 10-minute stability run.
3. Run `build/switch-dawn-probe/BlueWakeDawnOffscreenProbe.nro`. Confirm whether a Dawn GLES adapter is found, the quadrant and depth checks pass, the readback image appears, and **+** exits cleanly. Save `sdmc:/switch/wind-waker-recomp/dawn-probe.log` for diagnosis.
4. Do not treat the Dawn offscreen pass as proof of surface or Aurora support. Before implementing Aurora, write a bounded integration plan for SDL3/Horizon and the missing Dawn Switch-window surface, or present an explicitly approved alternative.
5. Only after the surface path is demonstrated should the team test Aurora/GX-generated shaders, texture sampling, blend/depth, EFB copy, and presentation, then consider game-composite integration.

## Build commands

- Bootstrap NRO: `bash scripts/switch/build_probe.sh`
- SDL2/GLES candidate NRO: `bash scripts/switch/build_gles_probe.sh`
- Dawn GLES offscreen NRO: `bash scripts/switch/build_dawn_probe.sh`

The bootstrap/GLES commands use the locally cached, digest-pinned devkitPro image when no native `DEVKITPRO` installation is present. The Dawn command uses the pinned-derived image in [Containerfile.dawn](../../scripts/switch/Containerfile.dawn). Resulting NROs and logs remain local and are not release artifacts.
