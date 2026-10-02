# Nintendo Switch Port — Implementation Checklist

**Updated:** 2026-10-02

**Scope:** Private, local homebrew build using the user's own supported disc. This checklist does not imply that Wind Waker itself runs on Switch.

## Completed

- [x] Defined the first visual target as the title screen/flyover and documented the phased Switch port plan in [SWITCH_PORT_PLAN.md](SWITCH_PORT_PLAN.md).
- [x] Built a libnx bootstrap NRO with the pinned, rootless devkitPro toolchain.
- [x] Fixed the duplicate `fsdevMountSdmc()` call. The corrected bootstrap uses libnx's automatic `sdmc` mount.
- [x] Confirmed from the supplied bootstrap log that SD is available, the log opens, and the probe stops cleanly.
- [x] Built the SDL2/Mesa GLES probe and confirmed its four-color textured test on physical Switch hardware.
- [x] Reviewed the supplied GLES logs: OpenGL ES 3.2, Mesa 20.1.0-rc3, renderer `NV120`, 1280×720 drawable, 16 depth bits, and framebuffer color-copy passed.
- [x] Recorded that the GLES runs were only 4.034 seconds and 2.351 seconds; no 10-minute soak or explicit depth-occlusion pixel test was demonstrated.
- [x] Cross-built the project's pinned Dawn OpenGLES backend as a separate offscreen Switch NRO. The experiment compiles WGSL, samples generated texture data, enables blend/depth, maps GPU readback, checks test pixels, and presents via the libnx framebuffer.
- [x] Investigated the first Dawn hardware failure. The EGL log reports EGL 1.4/Mesa Project, `EGL_KHR_surfaceless_context`, no robust-context extension, no EGL fence/reusable/native-fence extension, and no pbuffer config. Dawn rejected adapter discovery before rendering.
- [x] Added a diagnostic-only Switch fallback for the missing EGL synchronization extensions: `QueueGL` drains work with synchronous `glFinish` before advancing queue serials. Shared EGL fence export is unavailable in this fallback.
- [x] Updated the build and feasibility documentation. See [SWITCH_GRAPHICS_SPIKE.md](status/SWITCH_GRAPHICS_SPIKE.md) and [Dawn probe notes](../switch/dawn/README.md).
- [x] Verified the current NRO has `NRO0` at header offset `0x10`, passes local shell/whitespace checks, and remains ignored by Git.

## Current Dawn diagnostic artifact

- Path: [BlueWakeDawnOffscreenProbe.nro](../build/switch-dawn-probe/BlueWakeDawnOffscreenProbe.nro)
- Size: 11,489,280 bytes
- SHA-256: `355a1c98acc147c452773f7c1b296326ab8c246d062b502942bb68df3f28df90`
- Build command: `bash scripts/switch/build_dawn_probe.sh`
- Console log: `sdmc:/switch/wind-waker-recomp/dawn-probe.log`

The NRO has **not** been run after the latest changes. Cross-build success does not establish Dawn adapter creation, context creation, rendering, readback, or display success.

## Immediate pending tests

- [ ] Run the current Dawn NRO on the physical Switch and provide the resulting `dawn-probe.log`.
- [ ] Check the log for EGL capability details, OpenGLES adapter discovery, device/context creation, shader/pipeline creation, queue submission, mapped readback, four quadrant samples, depth-occlusion sample, and final `PASS`/`FAIL`.
- [ ] Confirm the result image appears on screen, **+** exits cleanly, and the NRO does not hang or crash.
- [ ] If adapter discovery succeeds but context creation fails, investigate Dawn's config/context path. The reported `surfaceless=yes` and `pbuffer_config=unavailable` need to be reconciled with the pinned Dawn context setup.
- [ ] Treat the `glFinish` and disabled-robustness modes strictly as diagnostic workarounds. They are synchronous, slow, do not support shared-fence export, and are not suitable for untrusted shaders or a production game build.
- [ ] Re-run the corrected GLES probe for 10 minutes and retain its log. Confirm the visible draw and framebuffer copy remain stable. The earlier short logs are not a soak test.

## Renderer gate — still open

- [ ] Establish a supported Dawn EGL/GL context path on physical hardware with the features Dawn needs.
- [ ] Demonstrate real Dawn window presentation to libnx `NWindow` or obtain approval for another bounded presentation design. The current framebuffer copy-out is not a Dawn surface.
- [ ] Resolve the pinned Aurora/SDL3 `NintendoSwitch` platform and thread-detection blocker, or present a reviewed alternative. Current Aurora configuration aborts in SDL3 before building the renderer.
- [ ] Exercise Aurora/GX-generated shaders and submissions on the target path.
- [ ] Validate texture sampling, blending, depth behavior, EFB/color copy, synchronization, and presentation on the console.
- [ ] Run a sustained stability test and record performance, memory, and device-loss behavior.
- [ ] Close Gate A only after the intended Aurora/GX graphics path—not merely a hand-written WGSL sample—passes on physical hardware.

## Deferred until Gate A passes

- [ ] Static-link the generated game composite into the Switch NRO and replace the current dynamic `dlopen`/`dlsym` provider path.
- [ ] Port the shared host runtime and Switch input, audio, storage, lifecycle, and disc-import services.
- [ ] Boot the user's own supported disc and display the title screen/flyover.
- [ ] Begin gameplay, feature parity, performance qualification, or release preparation.

## Distribution and data boundaries

- [x] Current probes contain generated test colors only; they do not include the game composite or game assets.
- [ ] Continue to exclude disc images, extracted game files/assets/textures, saves, memory-card images, personal settings, and third-party content from tracked artifacts and releases.
- [ ] Keep all user-provided game data and local generated files private and outside release packages, consistent with [AGENTS.md](../AGENTS.md).
