# Native port: The Wind Waker built from its decompilation

`native/tww/` holds the source of the Wind Waker decompilation: `src/`, `include/` and its license,
imported unchanged from [snrubrm/tww](https://github.com/snrubrm/tww) at `b09eebc` and changed only
by later commits in this repository. That fork builds on the work of the
[zeldaret/tww](https://github.com/zeldaret/tww) contributors and completes the remaining functions
with AI assistance; it is not part of upstream. Both are CC0-1.0 (`native/tww/LICENSE`).

At the import, the fork's own build (`configure.py`, Metrowerks compilers) reproduced the
player's GZLE01 revision 0 `main.dol` and all 415 RELs byte for byte (`416 files OK`, SHA-1
checked): every function has source, two units are "Equivalent" rather than byte-matching.

The source contains no game data. Some files include `assets/...` headers that the fork's build
generates from the player's disc; this port generates them the same way at build time, outside
the repository, under `build/`.

The goal is a native build of the game on Aurora: on the Mac first, then the Switch, following the
approach of [Dusklight](https://github.com/TwilitRealm/dusklight) (Twilight Princess, CC0), whose
SDK-over-Aurora layer and static REL linking are the reference. Why: the translated build runs at
about 6-8 percent speed on the Switch (`docs/SWITCH_IMPLEMENTATION_CHECKLIST.md`), and native code
costs about 0.9 host instructions per guest instruction against 27 for the translation.

## Building on the Mac (phase 1)

Phase 1 of `docs/NATIVE_PORT_PLAN.md`: every game unit compiles to an object with Apple clang
(arm64, C++20 / C11) under `TARGET_PC`, against the host C and C++ libraries instead of
Metrowerks' MSL. Nothing is linked yet.

Requirements: Xcode command line tools (Apple clang), CMake 3.25 or newer, Ninja.

```sh
cmake -S native -B build/native-mac -G Ninja
ninja -C build/native-mac tww_scaffold_check       # toolchain + base headers sanity check
```

Each module is an `OBJECT` library behind an option, off until it compiles; the modules listed
in `TWW_MODULES_READY` (`cmake/modules.cmake`) compile and default to on (currently `SSystem`, `JSystem-core`, `JSystem-J3D`, `JSystem-2D-particle`, `JSystem-studio`, `framework`, `m_Do`, `d-core`, `actors-1`, `actors-2`, `actors-3`, `actors-4`, `actors-5`, `actors-6`):

| Target | Option | Sources (`native/tww/src/...`) |
| --- | --- | --- |
| `SSystem` | `TWW_MODULE_SSystem` | `SSystem/**` |
| `JSystem-core` | `TWW_MODULE_JSystem_core` | `JSystem/{JKernel,JSupport,JUtility,JMath,JGadget,JFramework,JRenderer}` |
| `JSystem-J3D` | `TWW_MODULE_JSystem_J3D` | `JSystem/{J3DGraphBase,J3DGraphAnimator,J3DGraphLoader,J3DU}` |
| `JSystem-2D-particle` | `TWW_MODULE_JSystem_2D_particle` | `JSystem/{J2DGraph,JParticle}` |
| `JSystem-studio` | `TWW_MODULE_JSystem_studio` | `JSystem/{JStage,JMessage}`, `JSystem/JStudio/**` |
| `framework` | `TWW_MODULE_framework` | `f_pc`, `f_op`, `f_ap`, `c`, `DynamicLink.cpp` |
| `m_Do` | `TWW_MODULE_m_Do` | `m_Do` |
| `d-core` | `TWW_MODULE_d_core` | `d/*.cpp` |
| `actors-1` … `actors-6` | `TWW_MODULE_actors_N` | `d/actor`, sorted, in six equal chunks |

```sh
cmake -S native -B build/native-mac -DTWW_MODULE_framework=ON  # or -DTWW_ALL_MODULES=ON
ninja -C build/native-mac -k 0 SSystem                         # one module at a time
ninja -C build/native-mac tww_deferred                         # deferred units and reasons
```

Never part of the build in phase 1 (their headers may still be included): `src/dolphin` (the SDK
over Aurora is phase 2), `src/REL` (phase 3), `src/JSystem/JAudio` and `src/JAZelAudio` (phase 5),
`src/PowerPC_EABI_Support`, `src/TRK_MINNOW_DOLPHIN`, `src/OdemuExi2`, `src/odenotstub`,
`src/amcstubs`.

### Layout

- `native/CMakeLists.txt`: the project; `native/cmake/GameConfig.cmake`: definitions
  (`TARGET_PC=1`, `VERSION=2` for GZLE01, `NDEBUG=1`), include paths and flags, on the interface
  target `tww_game_headers` (Dusklight's `dusklight_game_headers`); `native/cmake/modules.cmake`:
  the modules; `native/cmake/deferred.cmake`: the units left for a later phase, each with its
  reason (`tww_defer(path "reason")`).
- `native/include/pc/tww_pc_config.h`, force-included in every unit: portable versions of the
  Metrowerks PowerPC intrinsics (`__cntlzw`, `__rlwimi`, `__dcbz`, `__sync`, `__fres`,
  `__frsqrte`) and MSL's float math in `std::` (`std::sqrtf`...).
- `native/include/pc/msl/`: shims for MSL-only header names (`algorithm.h`, `iterator.h`,
  `functional.h`, `new.h`) that include the host's standard headers.
- Changes to `native/tww` that differ for the original target are under `#if TARGET_PC` with the
  original code kept in the other branch, as in Dusklight.

### Asset headers

Units including `assets/...` or `res/Object/...` need the headers the decomp's build generates from
the player's disc. They go under `build/native-mac/assets/GZLE01/` (`include/assets/...` and
`res/Object/...`, the layout of the decomp's `build/GZLE01/include` and `assets/GZLE01`), never in
git; `-DTWW_ASSETS_DIR=` points elsewhere.

From `framework` on, most units need them (`d/d_com_inf_game.h` includes `res/Object/Always.h`).
With a built checkout of the decomp (`python configure.py && ninja` for GZLE01), copy them in:

```sh
mkdir -p build/native-mac/assets/GZLE01/include
cp -R <decomp>/assets/GZLE01/res build/native-mac/assets/GZLE01/
cp -R <decomp>/build/GZLE01/include/assets build/native-mac/assets/GZLE01/include/
```

## Aurora (phase 2)

Phase 2 builds the GameCube SDK over [Aurora](https://github.com/encounter/aurora) (MIT), the
library Dusklight uses: `native/cmake/Aurora.cmake` pulls it in with FetchContent at Dusklight's
pin, `3227d76`, behind `TWW_WITH_AURORA` (off by default until step 2.8 of
`docs/NATIVE_PORT_PHASE2_3.md`). As in Dusklight, GX, DVD, CARD and THP are on and `aurora_mtx` is
built with `MTX_USE_PS=1`; RmlUi, Aurora's examples and its tests are off. On darwin-arm64 Dawn and
nod come from Aurora's prebuilt packages (`AURORA_DAWN_PROVIDER` / `AURORA_NOD_PROVIDER` =
`package`), so neither a Dawn source build nor Rust is needed.

The first configure needs the network: Aurora (unless a local checkout is given), the Dawn and nod
packages, SDL3, abseil, fmt, xxhash, imgui and Tracy are fetched into the build directory. Aurora
takes libpng, Freetype, zlib, SQLite and zstd from the system (Homebrew) when found.

```sh
cmake -S native -B build/native-mac -G Ninja -DTWW_WITH_AURORA=ON
ninja -C build/native-mac aurora_core aurora_gx aurora_gd aurora_os aurora_vi aurora_pad \
    aurora_si aurora_mtx aurora_dvd aurora_card aurora_thp aurora_main
```

With a local clone of Aurora that contains the pin (such as `ref/aurora`), use CMake's own
override instead of cloning; the configure warns if that checkout is not at the pin:

```sh
git -C ref/aurora worktree add --detach "$PWD/build/aurora-3227d76" 3227d76
cmake -S native -B build/native-mac -G Ninja -DTWW_WITH_AURORA=ON \
    -DFETCHCONTENT_SOURCE_DIR_AURORA="$PWD/build/aurora-3227d76"
```

The SDK libraries the game will link are listed in `TWW_AURORA_LIBS`. The game flags
(`TARGET_PC`, `-fno-exceptions`, the force-included PC config header) live on the interface target
`tww_game_headers` and never reach Aurora.

`native/sdk` holds the TWW-specific SDK over Aurora: the static library `tww_sdk` and its headless
test `tww_sdk_smoke` (`native/cmake/sdk.cmake`, built only with `TWW_WITH_AURORA=ON`); see
`native/sdk/README.md`.

### SDK headers (`TWW_SDK_HEADERS`)

Which SDK headers the game compiles against is the cache variable `TWW_SDK_HEADERS`
(`cmake/GameConfig.cmake`, decision D2 of `docs/NATIVE_PORT_PHASE2_3.md`):

- `decomp` (the default until step 2.8): the decomp's own `native/tww/include/dolphin`, as in
  phase 1.
- `aurora` (needs `TWW_WITH_AURORA=ON`): Aurora's headers are the only SDK headers. The include
  order is `native/include` → `native/include/sdk` → Aurora's `include` → `native/tww/include`.
  Aurora wins every header name both have (39 of the decomp's 82), so Aurora's own includes stay
  consistent. `native/include/sdk/dolphin/**` holds forwarders for the names only TWW has
  (`dolphin/os/OS.h` → `<dolphin/os.h>` plus the TWW-only declarations Aurora lacks), and
  `native/include/sdk/tww_sdk_extras.h`, force-included in every game unit, restores what the
  decomp's `dolphin/types.h` had beyond Aurora's (`uint`, `READU32_BE`, `FLOAT_MIN`/`FLOAT_MAX`).
  Hardware registers the decomp defines in its headers (`__VIRegs`, `OS_PI_INTR_*`...) are left
  out on purpose. Until step 2.7 has migrated a module, it does not compile in this mode.

`tww_sdk_header_check` compiles `check/sdk_headers.cpp`, which includes every SDK header name the
decomp has, in either mode. In aurora mode, `tww_sdk_shadow_check` runs
`check/check_sdk_shadow.sh`: it fails if any dependency of that unit resolves under
`native/tww/include/dolphin`, or if a name there is missing from the unit. Since step 2.4 every
one of the 82 names has a forwarder or an Aurora header, and none is pending. TWW-only GF
declarations (`GFLoadPosMtxImm`, `GFSetArray`, `GFBegin`...) live in
`native/include/sdk/tww_gf_extras.h`, which the `GF.h` and `GFTransform.h` forwarders include.

```sh
cmake -S native -B build/native-mac -G Ninja -DTWW_WITH_AURORA=ON -DTWW_SDK_HEADERS=aurora
ninja -C build/native-mac tww_scaffold_check tww_sdk_header_check tww_sdk_shadow_check
```
