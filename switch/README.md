# Switch bootstrap probe

This is the first implementation slice for the private Switch NRO effort. It
checks three prerequisites on a physical console: that a libnx NRO launches,
that the libnx framebuffer console can present text, and that the app can use
the automatically mounted SD storage and append a log. It **does not** link the recompiled game, Aurora,
Dawn, or a GX renderer; its screen is diagnostic text, not Wind Waker graphics.

## Build

The supported quick path uses the official devkitPro `devkita64` container,
pinned by image digest and run rootlessly with Podman. It was pulled and used
to build the probe in this workspace. Build with:

```sh
bash scripts/switch/build_probe.sh
```

The script uses a local devkitPro install if `DEVKITPRO` is set (or
`/opt/devkitpro` exists); otherwise it uses the pinned container. To install a
native toolchain, follow the [official devkitPro setup guide](https://devkitpro.org/wiki/Getting_Started)
and install `switch-dev`, then set `DEVKITPRO` if needed. The Makefile writes
intermediates and the NRO beneath the repository's ignored
`build/switch-probe/` directory and fails early if libnx's `switch_rules` are
missing.

## On-console check

Copy `build/switch-probe/BlueWakeSwitchProbe.nro` to the user's existing
homebrew launcher location and launch it using their already-configured,
lawful homebrew setup. Normal libnx NROs have `sdmc` mounted automatically, so
the probe checks for that device instead of mounting it a second time. An
explicit duplicate `fsdevMountSdmc()` returned `0x00000559` on the first test;
that decodes to `Module_Libnx / LibnxError_OutOfMemory`, which libnx returns
when its device-registration step fails. The corrected probe displays SD
availability and log creation status. It appends a short status record to
`sdmc:/switch/wind-waker-recomp/boot-probe.log`; press **+** to exit.

This passes only the NRO boot/storage/display-plumbing substep. The next gate
is a hardware proof that Aurora-generated GX work—including a textured draw,
depth/blend, and an EFB copy—can be presented. A successful probe is not
evidence that the game renderer or the game itself runs on Switch.

## SDL2 / Mesa GLES feasibility probe

The devkitPro toolchain also includes the public Switch SDL2 and Mesa EGL/GLES
ports. Build a separate on-console smoke test for that candidate stack with:

```sh
bash scripts/switch/build_gles_probe.sh
```

This produces `build/switch-gles-probe/BlueWakeGlesProbe.nro`. It opens an
SDL2 OpenGL ES context, compiles a small shader, draws an in-memory textured
quad with blending and depth enabled, and checks a color framebuffer copy.
Its log is written to `sdmc:/switch/wind-waker-recomp/gles-probe.log`; **+**
exits. It includes no game data and is not an Aurora/Dawn test. Passing it
only establishes that this public SDL2/Mesa GLES route works on the console;
Aurora currently requires SDL3 and Dawn, so the graphics gate remains open.
See the [graphics feasibility record](../docs/status/SWITCH_GRAPHICS_SPIKE.md)
for the current assessment and next gate.

## Dawn OpenGLES offscreen probe

Build the standalone Dawn test with:

```sh
bash scripts/switch/build_dawn_probe.sh
```

This produces `build/switch-dawn-probe/BlueWakeDawnOffscreenProbe.nro`. On the
console, Dawn creates an EGL-backed OpenGLES adapter, renders a generated
textured WGSL scene with alpha blending and depth testing, copies the offscreen
color texture to a mapped buffer, checks the four colors and center depth
result, then presents the readback through libnx's framebuffer. The log is
written to `sdmc:/switch/wind-waker-recomp/dawn-probe.log`; press **+** to exit
after the result screen appears. The test includes no game data or assets.

This tests Dawn plus Mesa GLES only if it passes on a physical console. It does
not add the missing Dawn/NWindow surface path, test Aurora GX, or render Wind
Waker. See the [Dawn probe notes](dawn/README.md) and the
[graphics feasibility record](../docs/status/SWITCH_GRAPHICS_SPIKE.md).

The probe now logs EGL version, vendor, extension strings, robust-context,
fence-sync, and pbuffer support. If robust-context support is absent, it turns
off Dawn's robust-buffer-access toggle for this fixed diagnostic shader only;
that fallback is not appropriate for a production renderer. The first run
failed at adapter discovery with `EGL_EXT_create_context_robustness is
required`, before Dawn rendered anything; rerun this revised binary and retain
the new `dawn-probe.log`.

The follow-up log also reported no EGL fence-sync, reusable-sync, or Android
native-fence extension. The diagnostic build now serializes Dawn submissions
with `glFinish` when none are present; this is slow, provides no shared-fence
export, and is **not** a production renderer design. The log reports
surfaceless-context support but no pbuffer config, so the rerun must verify
Dawn's EGL config/context creation as well as adapter discovery.