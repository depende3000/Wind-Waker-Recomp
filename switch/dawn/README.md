# Dawn OpenGLES offscreen probe

**Build status (2026-10-02):** the Switch NRO cross-compiles and packages
successfully. A first hardware run found that Mesa does not advertise
`EGL_EXT_create_context_robustness`, so Dawn rejected adapter discovery before
rendering. The rebuilt diagnostic now logs EGL version/vendor/extension strings
and explicit robustness, fence-sync, and pbuffer capability flags. The latest
log also shows no `EGL_KHR_fence_sync`, `EGL_KHR_reusable_sync`, or Android
native-fence extension. The Switch-only diagnostic therefore adds a
synchronous `glFinish` queue fallback, completing each Dawn serial before
returning. This is intentionally slow and does not support shared EGL fence
export. It uses robust WebGPU buffer access when the display supports robust
contexts; if not, it disables that toggle only for its fixed, in-memory test
shader. This diagnostic workaround is **not safe for a production build or
untrusted shaders**. The revised NRO still needs a physical rerun; adapter
creation, `glFinish` queue operation, rendering, pixel checks, and surfaceless
context behavior remain unverified. The log says surfaceless context is
available, so Dawn should not need the unavailable pbuffer surface, but its
EGL config selection still needs to be validated on-device.

This is the next isolated graphics feasibility experiment. It builds Dawn from
the commit pinned by the project's vendored Aurora CMake setup
(`266c1cf8de969a364afa4fa49311631fc99a881e`) and verifies the downloaded source
archive with SHA-256
`03ca9de39e1b534c9a443ede66ce8fcf61521edfa7d526f9356972241cbd957d`.

The NRO asks Dawn's native OpenGLES backend to initialize EGL from
`eglGetProcAddress`, compiles a WGSL textured draw, enables alpha blending and a
depth attachment, copies the offscreen color texture to a mapped buffer,
checks all four texture colors plus depth occlusion, and presents the pixels
through libnx's linear framebuffer. Its log is written to
`sdmc:/switch/wind-waker-recomp/dawn-probe.log`. The test uses generated colors
only; it contains no game data.

## Build

Run `bash scripts/switch/build_dawn_probe.sh`. The script uses a compatible
local devkitPro/CMake/Ninja installation when available; otherwise it
builds and runs the pinned devkitPro-derived container in
`scripts/switch/Containerfile.dawn`. Dawn's own dependency-fetch script checks
out the revisions in the pinned Dawn `DEPS` file. Build output stays under the
ignored `build/switch-dawn-probe/` directory.

The latest NRO is 11,489,280 bytes with SHA-256
`355a1c98acc147c452773f7c1b296326ab8c246d062b502942bb68df3f28df90`.
Rebuilding after source or toolchain changes can produce a new checksum; use
the build script's output as the current value.

## Switch-only compatibility patches

The patches in `switch/dawn/patches/` are applied only to the ignored Dawn and
Abseil `FetchContent` sources beneath `build/`. They add Horizon/POSIX platform
identity, EGL opaque handles for offscreen use, libnx sleep, and static-NRO
behavior where upstream code assumes `dlfcn`; the Abseil patches adapt its
thread, ELF, and timezone code to newlib, and the Tint/Dawn patches replace
libc-only calls missing from devkitA64. One Dawn discovery patch permits the
diagnostic to proceed without EGL robust-context support, and another
recognizes EGL native-fence sync; if no EGL sync extension exists, a separate
Switch-only queue patch serializes submissions with `glFinish`. The latter is
diagnostic-only, slow, and does not support shared-fence export. The robustness
bypass is paired with Dawn's `disable_robustness` device toggle. These changes
do not add a Switch window surface or alter non-Switch builds, and neither
fallback is appropriate for a real game/production build.

Copy `build/switch-dawn-probe/BlueWakeDawnOffscreenProbe.nro` to the console's
existing homebrew launch location. A successful test fills the display with
the Dawn-readback color quadrants and a green center; press **+** to exit. A
red patterned screen indicates failure; inspect the SD log for the first Dawn,
EGL, shader, pipeline, readback, or pixel-check error. This is only an
offscreen Dawn+GLES proof. It does not demonstrate Aurora GX, Dawn window
presentation, or Wind Waker rendering.