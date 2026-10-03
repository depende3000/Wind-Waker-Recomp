# Building the Switch NRO

How to build the unofficial Nintendo Switch homebrew build of Wind Waker Recomp from your own disc,
copy it and its data to the console, and read its logs. There are two builds:

- **headless** (the default): the game runs without graphics, sound or controls and reports its
  progress on screen and in a log. It proves the recompiled game code and runtime on the console.
- **Aurora** (`--aurora`): with the GX renderer, the controller, sound and rumble. Aurora itself
  runs on the console (picture through Dawn's OpenGL ES backend at 60 FPS, the controller through
  the GameCube PAD API, sound and HD rumble); the game with Aurora has not run yet.

Plan and status:
[SWITCH_PORT_PLAN.md](SWITCH_PORT_PLAN.md), [SWITCH_IMPLEMENTATION_CHECKLIST.md](SWITCH_IMPLEMENTATION_CHECKLIST.md).

> [!IMPORTANT]
> The NRO contains code translated from your disc. Like every build in this repository, it is for
> your own console only: never share or upload it, the disc image or any file extracted from it
> ([AGENTS.md](../AGENTS.md)). Everything below stays in the ignored `build/` and `ref/` folders.

## What you need

- A Mac (Apple silicon) or Linux machine with Git, Python 3, clang, CMake and Ninja.
  On macOS, Xcode's clang and `brew install cmake ninja libmtp libusb uv`.
- Docker Desktop or Podman. The scripts use the official devkitPro image, pinned by digest. It runs
  natively on Apple silicon (`linux/arm64`). Give it at least 8 GB of memory (Settings › Resources).
- Your disc image of *The Legend of Zelda: The Wind Waker*, GameCube USA (`GZLE01`, revision 0), as an
  uncompressed `.iso`. The builder checks it and refuses other versions.
- A Switch you have already set up for homebrew (Atmosphère and the Homebrew Menu), a USB-C data
  cable, and the console's **USB file transfer** (Horizon's own, or haze/DBI).

## 1. Build

```sh
scripts/bootstrap.sh                                         # pinned RecompCore, DolRecomp, Aurora into ref/
scripts/builder/build.sh /path/to/GZLE01.iso --source-only   # check the disc, generate the game source
scripts/switch/build_host.sh                                 # build/switch-host/BlueWakeSwitch.nro
scripts/switch/build_host.sh --aurora                        # build/switch-host-aurora/BlueWakeSwitch.nro
```

- `--source-only` extracts `main.dol` and the 415 RELs into `build/device/game` and generates the
  translated source into `build/device/composite-src` (about 900 MB of C). It takes a few minutes
  and ends with `composite source digest …: the verified tree`.
- `build_host.sh`:
  1. compiles that source on the computer itself with clang for the Switch's CPU, against
     devkitA64's newlib headers (`switch/composite/clang-switch.cmake`). clang takes about 1.7x
     less time than devkitA64's GCC on these files and half the memory (1.5 GB for the largest);
  2. in the container, links those objects into one relocatable object,
     `build/switch-composite-clang/gGZLE01_recomp.o`, with every symbol renamed `bwc_<name>`;
  3. links it with the host, GXRuntime and the donor DSP (and, with `--aurora`, Aurora and Dawn)
     into the NRO (`switch/host`).

  The first run takes about an hour and a half on a 10-core Mac; later runs reuse what is already
  compiled. The first `--aurora` build also compiles Dawn. `SWITCH_BUILD_JOBS` sets the parallel
  jobs (default: every core for the composite, 4 in the container).

If your disc image is compressed (`.ciso`, `.rvz`, …), convert it to `.iso` first. A GameCube `.iso`
is exactly 1,459,978,240 bytes.

## 2. Copy to the console

Turn on USB file transfer on the console, connect it to the computer, then:

```sh
scripts/switch/push.sh --game /path/to/GZLE01.iso   # game data: about 1.5 GB, 82 s the first time
scripts/switch/push.sh host                         # the NRO, read back and checked by SHA-256
scripts/switch/push.sh host-aurora                  # or the Aurora build, under the same name
```

`push.sh` copies over MTP and needs no SD-card reader or reboot. `--game` skips files that are
already on the console with the same size, so it is quick to rerun. The resulting SD-card layout:

| Path on the SD card | Contents | Source |
|---|---|---|
| `switch/wind-waker-recomp/BlueWakeSwitch.nro` | the app | `build/switch-host/BlueWakeSwitch.nro` |
| `switch/wind-waker-recomp/GZLE01.iso` | your disc image; the game reads its data from it | your `.iso` |
| `switch/wind-waker-recomp/dsp_rom.bin`, `dsp_coef.bin` | free replacement DSP ROMs | `ref/recompcore/Data/Sys/GC/` |
| `switch/wind-waker-recomp/game/main.dol` | the game's executable | `build/device/game/main.dol` |
| `switch/wind-waker-recomp/game/rels/*.rel` | the game's 415 modules | `build/device/game/rels/` |
| `switch/wind-waker-recomp/GZLE01.card` | memory card; created by the game | — |
| `switch/wind-waker-recomp/host.log`, `states/` | log and save states; created at run time | — |

Without USB file transfer, copy the same files with an SD-card reader, or with Hekate's
**Tools › USB Tools › SD Card**, to the paths above.

## 3. Run

Start the Homebrew Menu in **title mode**: hold **R** while you open an installed game. Launching it
from the Album gives applets far less memory than the game needs. Open **BlueWakeSwitch**.

The headless build:

- shows no game picture: the screen shows its log as text, the same lines as `host.log` and the
  live USB log;
- runs about one minute of game time (`BLUEWAKE_MAX_RETRACES=3600`) and then writes
  `[switch] host returned …`;
- exits with **+**.

It checks for the DOL, disc and DSP ROMs first, and logs any that are missing.

## 4. Logs and crashes

- **Live over USB.** Leave the cable connected and run, on the computer:

  ```sh
  uv run scripts/switch/usb_log.py --out build/switch-logs/live.log
  ```

  It waits for the app and prints its log as the app writes it. It runs directly on the host, not
  in a container (Docker Desktop has no USB access).
- **From the SD card.** Every line also goes to `switch/wind-waker-recomp/host.log`. With USB file
  transfer on, `scripts/switch/push.sh --logs` copies it, and the probes' logs, to
  `build/switch-logs/`.
- **Crashes.** Atmosphère writes a report to `atmosphere/crash_reports/`. Copy it with
  `build/switch-tools/switch_mtp pull atmosphere/crash_reports <name>.log out.log`, then resolve its
  addresses (`+ 0x…` after the module name) with the ELF next to the NRO:

  ```sh
  aarch64-none-elf-addr2line -f -C -i -e build/switch-host/bluewake_switch_host.elf 0x…
  ```

  `aarch64-none-elf-addr2line` is in the devkitPro image. Run it through `docker run` with the
  repository mounted, as the build scripts do.

## Native port

The native port (`native/`: the game built from its decompilation on Aurora, see
[NATIVE_PORT_PLAN.md](NATIVE_PORT_PLAN.md)) has its own NRO, `TwwNative.nro`, built from the same
toolchain image, Aurora/Dawn-for-Switch build, SDL 3 shim, SD card folder and logs as the translated
port. It reads only the disc image from the SD card: the game's code is compiled in, and the asset
headers are compiled in at build time, from the same `TWW_ASSETS_DIR` as the Mac build. It has not
run on a console yet (phase 7 of the plan).

### Build

Needs what the Mac build of `native/` needs (Aurora at the pin in `build/aurora-3227d76`, the asset
headers in `build/native-mac/assets/GZLE01`, `ref/recompcore`; [native/README.md](../native/README.md))
plus Docker Desktop or Podman:

```sh
scripts/switch/build_native.sh       # build/switch-native/TwwNative.nro and tww.elf
```

- It builds `localhost/wwrecomp-switch-native-build:2026-10-03` the first time
  (`scripts/switch/Containerfile.native`: the pinned devkitPro image of the translated port plus
  Debian's clang 19), then configures `switch/native` with devkitPro's Switch toolchain and builds the
  `tww_nro` target in `build/switch-native`.
- Aurora, Dawn, the SDK (`native/sdk`), Dolphin's DSP HLE and libnx compile with devkitA64's GCC; the
  game units and the run harness compile with clang 19 for the Cortex-A57
  (`switch/native/clang-launcher.sh`), as they do with Apple clang on the Mac.
- Aurora is `build/aurora-3227d76` copied into the build directory with `switch/native/aurora/patches`
  applied (the window surface on libnx's NWindow through Dawn's OpenGL ES backend, `gl_defer`, ImGui
  without SDL's backends), the mechanism `switch/aurora` uses for the translated port. Dawn is the
  translated port's (`switch/dawn`, encounter/dawn `266c1cf` with its Horizon patches), the one that
  has presented Aurora's frames on the console; the Dawn source fetched by the translated port's Dawn
  probe (`build/switch-dawn-probe/_deps/dawn-src`) is reused when it is there.
- nod (Aurora's disc reader, written in Rust) has no libnx target: `switch/native/nod` reads plain
  GameCube `.iso` images behind nod's C API. On the Mac it gives the same file system table, metadata
  and file contents as nod for GZLE01, and a Mac `tww` linked with it passes `disc-ls`, `arc-sweep`,
  `stage-sweep`, `j3d-sweep` and `opening`.
- The first build fetches Dawn's dependencies (unless the probe's source is there), SDL 3's headers,
  ImGui, Tracy, fmt, xxhash and sqlite, and compiles Dawn and the game: about 40 minutes with 4 jobs
  on a 10-core Mac when the probe's Dawn source is reused; later builds take minutes. `--jobs N` (or
  `SWITCH_BUILD_JOBS`) sets the parallel jobs, default 4. `--aurora`, `--assets`, `--recompcore` and
  `--dawn-src` point at other copies of the inputs; from a git worktree (`build/lanes/<lane>`) the
  main checkout's are used.
- Output: `build/switch-native/TwwNative.nro` (about 21 MB) and `build/switch-native/tww.elf`, the
  same program with its symbols, for `addr2line`. Keep the ELF of the NRO you test.

### Copy to the console

```sh
scripts/switch/push.sh --disc /path/to/GZLE01.iso   # once: the disc image (skipped if already there)
scripts/switch/push.sh native                       # the NRO, read back and checked by SHA-256
scripts/switch/push.sh --native-env my-env.txt      # optional: run options (see below)
```

`--game` (the translated port's data) puts the disc image in the same place, so `--disc` is not
needed after it. SD card layout:

| Path on the SD card | Contents |
|---|---|
| `switch/wind-waker-recomp/TwwNative.nro` | the app: "The Wind Waker (native)" in the Homebrew Menu |
| `switch/wind-waker-recomp/GZLE01.iso` | your disc image, shared with the translated port |
| `switch/wind-waker-recomp/native/env.txt` | optional run options |
| `switch/wind-waker-recomp/native/tww.log`, `tww.prev.log` | this run's log and the previous one's |
| `switch/wind-waker-recomp/native/user/` | memory card (`USA/Card A`), Aurora's caches |

### Run

Start the Homebrew Menu in title mode (hold **R** while opening an installed game; an applet has far
less memory than the game needs, and the log says so) and open **The Wind Waker (native)**. The CPU
stays at its stock 1020 MHz. With USB connected, `uv run scripts/switch/usb_log.py --out
build/switch-logs/native-live.log` shows the log live.

Run options come from `native/env.txt`, one `NAME=value` per line, with `#` comments
([switch/native/env.example.txt](../switch/native/env.example.txt)); they are the Mac's `TWW_*`
variables ([native/README.md](../native/README.md), "Running tww"). Without the file:
`TWW_DISC=/switch/wind-waker-recomp/GZLE01.iso`, `TWW_RUN_DIR=/switch/wind-waker-recomp/native`,
`TWW_PERF_EVERY=60` and `TWW_STALL_S=90`.

What the log shows, in order (the same `[tww]` lines as on the Mac; values vary):

```
[switch] The Wind Waker, native port (phase 7); argv[0]=sdmc:/switch/wind-waker-recomp/TwwNative.nro
[switch] tww native: application (title mode); memory 3xxx MiB, ... core mask 0x7; image at 0x...
[switch] logs: /switch/wind-waker-recomp/native/tww.log open, USB live log started
[tww] harness: smoke=- milestone=- timeout=0s stall=90s ...
[tww] perf: game-thread frame times every 60 frames (TWW_PERF_EVERY)
[tww] disc: /switch/wind-waker-recomp/GZLE01.iso GZLE01 revision 0, 1459978240 bytes
[info] [aurora::gpu] Attempting to initialize OpenGLES          <- Dawn on Mesa (NV120)
[tww] aurora: backend=opengles window=1280x720 ...
[tww] dvd: GZLE01 version 0 disc 0                               <- the disc is read through nod_gcn
[tww] MILESTONE aurora-up ...
[tww] heaps: root ... check ok   (six heaps)
[tww] MILESTONE heaps ...
[tww] gfx-create: LOAD_COPYDATE status 1, COPYDATE "03/02/19 11:43:53"
[tww] MILESTONE gfx-create ...
[tww] frame loop: start, paced by JFWDisplay
[tww] audio: mDoAud_Create done at frame N; DSP handshake done
[tww] MILESTONE logo-scene ...                                   <- the Nintendo logo is on screen
[tww] perf frames 1-60: game thread X ms avg, Y ms max (begin B, aurora_end_frame E); pace wait W ms avg; F fps, R retraces/s (60 = full speed)
[tww] MILESTONE frame-loop ...
[tww] logo-res: all commands synced at frame ...: 26 archives mounted, 4 files in main RAM, 0 empty
[tww] MILESTONE logo-res ...
[tww] stage: sea_T room 44 created at frame ...; Stage archive 23 files, stage.dzs found
[tww] MILESTONE opening ...                                      <- the title's sea
```

Then the game goes as far as the Mac build of the same commit: at `e0b30df` both stop in the title
demo at about frame 301 with `[tww] PANIC in ".../d_a_player_main.cpp" on line 9342` (exit 12, the
next root cause of milestone M8 on the Mac). The app ends with `[switch] exit <code> ...; ending
the process` and returns to the HOME menu (the game's threads cannot be stopped, so the process ends
instead of returning to the Homebrew Menu). Exit codes are the Mac's (native/README.md).

The `[tww] perf` lines are the speed at 1020 MHz: "game thread" is the game's own work per frame
(the frame minus the wait for the next tick), "begin" includes waiting for Aurora's render worker,
and "retraces/s" is the game's speed (60 is full speed; the game asks for a frame every one or two
retraces). Threads: the game thread runs on core 0; JAudio's, the DVD thread, Aurora's and Dawn's
workers prefer cores 1 and 2 (`switch/native/source/thread_wrap.c`). Every 15 seconds, at exit
and in a crash report, `[switch] memory: used N MiB of M MiB` shows the process's memory.

### Crashes

- **The log.** A crash prints `[tww] CRASH <kind> esr=... far=...`, the registers, and a backtrace
  with every address also given as `tww.elf+0x<offset>`; then the harness's state line (scene,
  frame, last resource). `abort()` (Aurora's fatal errors, asserts) prints `[tww] ABORT` with a
  backtrace the same way, and an `OSPanic` `[tww] PANIC` (exit 12). Get the log with
  `scripts/switch/push.sh --logs` (to `build/switch-logs/native/tww.log`), or from the live USB log.
  Resolve the offsets with the ELF of the same build:

  ```sh
  docker run --rm -v "$PWD/build/switch-native:/b" localhost/wwrecomp-switch-native-build:2026-10-03 \
      /opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line -f -C -i -e /b/tww.elf 0x<offset> ...
  ```

- **Atmosphère's report.** After the log, the crash goes on to Atmosphère, which writes
  `atmosphere/crash_reports/<time>_<program id>.log`. Copy it with
  `build/switch-tools/switch_mtp pull atmosphere/crash_reports <name>.log out.log` and resolve the
  addresses after the module name (`+ 0x...`) the same way.
- `TWW_SMOKE=crash-test` in `env.txt` crashes on purpose, to see both reports once.

### If something goes wrong

- Nothing in the log at all: check that `switch/wind-waker-recomp/native/` exists afterwards (the
  app creates it); start from title mode.
- `[tww] DISC: cannot open TWW_DISC=...` (exit 14): the disc image is missing; `push.sh --disc`.
- The app closes right after `Attempting to initialize OpenGLES`: Dawn or Mesa failed; the Atmosphère
  report and the last `[info] [aurora::gpu]` lines say where.
- `[tww] STALL: frame counter frozen` (exit 11): no game frame for 90 seconds (`TWW_STALL_S`).
- Every run aborts at the same point right after start-up, after one that aborted in a shader:
  Aurora recompiles its cached pipelines at start-up (as on the Mac, phase 6 render issues); delete
  `switch/wind-waker-recomp/native/user/cache/`.
- Docker Desktop on macOS needs access to the folder the repository is in: if `build_native.sh` hangs
  with its container in the "Created" state, allow Docker in System Settings › Privacy & Security ›
  Files and Folders (Documents), or restart Docker Desktop.

## Probes

The graphics-feasibility probes live in `switch/` and are built and copied the same way:

```sh
scripts/switch/push.sh --build dawn gles boot
```

See [switch/README.md](../switch/README.md) and [the graphics spike](status/SWITCH_GRAPHICS_SPIKE.md).
