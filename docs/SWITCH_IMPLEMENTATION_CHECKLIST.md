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
- [x] Made the Dawn patch set apply to a clean checkout. `abseil-switch-timezone-newlib.patch` and `dawn-switch-allow-native-fence-sync.patch` had been written against intermediate local edits; both are now regenerated against the sources they are applied to, and the unused `abseil-switch-timezone.patch` is removed.
- [x] Rebuilt all three probes from a clean checkout with Docker on Apple Silicon. The bootstrap NRO reproduces the recorded `39cc9639…` hash. The GLES NRO builds deterministically as `4ca067b1…`, not the recorded `23ccf3cb…`, so that recorded hash predates the committed `gles_probe.c`.
- [x] Ran the `c153fe36…` Dawn NRO on hardware. Dawn found the OpenGLES adapter (`NV120`, ES 3.2) and created the device with robustness disabled and no EGL sync. Three probe bugs then stopped it: the test WGSL called `textureSample` in non-uniform control flow, the readback used a timed `WaitAny` without the `TimedWaitAny` instance feature, and the instance/adapter wrappers were `Acquire`d over references the `dawn::native` objects still owned, so they were released twice and the app closed before logging a result. All three are fixed in the `79c01d0c…` build, which still needs a physical rerun.
- [x] The latest GLES run reached 60.4 FPS with framebuffer copy passing, but lasted 9.8 seconds; the 10-minute soak is still outstanding.
- [x] Added a live USB log: `switch/source/common/usb_log.c` sends probe output through libnx usbComms (`057e:3000`) without ever blocking the probe, and `scripts/switch/usb_log.py` prints it on the host. The Dawn probe also builds with `-g` so its ELF can symbolize crash reports.
- [x] Added `scripts/switch/push.sh`: copies NROs to `sdmc:/switch/wind-waker-recomp/` over the console's USB file transfer (MTP), reads each back to check its SHA-256, and pulls probe logs with `--logs`.
- [x] Fixed the last probe bug: compatibility mode rejects `@interpolate(flat)` (implicitly `flat, first`), so the shader uses `flat, either`.
- [x] **Dawn OpenGLES offscreen probe passes on physical hardware** (`87f88dd2…`, two runs, live USB log). Quadrants read back 255,0,0 / 5,138,20 / 5,10,148 / 255,255,0 — the two 50%-alpha quadrants match the expected blend with the clear color exactly — and the green quad occludes the red one (center 0,255,0). The app presents the readback and exits cleanly. This runs with robustness disabled and a `glFinish` per submission; it is not Aurora/GX, not a Dawn surface, and its cost is unmeasured.
- [x] Retired the Aurora audit's top risks on hardware (`70ecef08…`):
  - **Limits:** 16 storage buffers in the vertex stage (Aurora needs 2), 2D textures to 16384, 256-byte uniform alignment.
  - **Threads:** Dawn's GL context works only on the thread that created the device. Drawing from another thread loses the device, and `gl_allow_context_on_multi_threads` is broken on this Mesa: with it, nothing is drawn even on one thread. A worker thread that creates the device and does all of the GPU work passes. Aurora on Switch therefore needs a single GPU thread without that toggle.
  - **Stacks:** libnx gives `std::thread` 128 KiB, which Tint's WGSL parser overflows (Atmosphère crash report 2168-0002 in `tint::resolver`). Threads that create shader modules need several MiB; the probe uses 4 MiB.
  - **Cost:** 300 frames at 960x720 with readback, `glFinish` per submission: 9.07 ms average, 10.51 ms slowest. About 24 ms of a 33 ms frame remain for GX work.
- [x] Dawn's `gl_defer` toggle works on the console (scenario E): with all GL work deferred to `Queue::Submit` and one context bound only while it runs, a device created on one thread draws from another, at 9.01 ms per 960x720 frame with readback. Aurora can keep its own threads; it does not need a single GPU thread.
- [x] Aurora builds and links for the Switch (`BLUEWAKE_SWITCH_AURORA`, `switch/aurora`), with the host, GXRuntime, Dawn and the donor DSP in one NRO, so far against a synthetic composite. SDL 3 comes from a small libnx shim of the 134 functions Aurora calls (`switch/aurora/sdl3_shim`): gamepad over HID, events, one fixed window, SD-card I/O, and absent audio, haptics, sensors, keyboard and mouse. Three source patches (`switch/aurora/patches`): a designator order GCC rejects, the stall-stack watchdog off as on Windows, and ImGui without its SDL backends. Every thread gets a 4 MiB stack through a `pthread_create` wrapper.

## Current Dawn diagnostic artifact

- Path: [BlueWakeDawnOffscreenProbe.nro](../build/switch-dawn-probe/BlueWakeDawnOffscreenProbe.nro)
- Size: 11,489,280 bytes
- SHA-256: `87f88dd27d5899c595cc5ad274cf116bdb9bf04d7bf9d7df36e852402d7e9acb` (**PASS on hardware**, 2026-10-02)
- Build command: `bash scripts/switch/build_dawn_probe.sh`
- The build scripts use Podman, or Docker when Podman is absent (`SWITCH_CONTAINER_ENGINE` forces one). The pinned devkitPro image has a native `linux/arm64` variant.
- Console log: `sdmc:/switch/wind-waker-recomp/dawn-probe.log`

The NRO has **not** been run after the latest changes. Cross-build success does not establish Dawn adapter creation, context creation, rendering, readback, or display success.

## Immediate pending tests

- [x] Run the current Dawn NRO on the physical Switch and provide the resulting `dawn-probe.log`.
- [ ] Check the log for EGL capability details, OpenGLES adapter discovery, device/context creation, shader/pipeline creation, queue submission, mapped readback, four quadrant samples, depth-occlusion sample, and final `PASS`/`FAIL`.
- [x] Confirm the result image appears on screen, **+** exits cleanly, and the NRO does not hang or crash.
- [ ] If adapter discovery succeeds but context creation fails, investigate Dawn's config/context path. The reported `surfaceless=yes` and `pbuffer_config=unavailable` need to be reconciled with the pinned Dawn context setup.
- [ ] Treat the `glFinish` and disabled-robustness modes strictly as diagnostic workarounds. They are synchronous, slow, do not support shared-fence export, and are not suitable for untrusted shaders or a production game build.
- [x] GLES soak: 1,286 s (21.4 minutes), 72,019 frames at a steady 60 FPS, framebuffer copy passing throughout, clean exit. Two pauses of 31 s and 60 s (the app in the background) resumed at 60 FPS on their own.

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
