# Game compile configuration, the native counterpart of the decomp's configure.py cflags and of
# Dusklight's cmake/GameABIConfig.cmake. Everything the game units share goes on the INTERFACE
# library tww_game_headers; each module links it.
include_guard(GLOBAL)

# GZLE01 is VERSIONS.index("GZLE01") == 2 (VERSION_USA) in the decomp's configure.py.
set(TWW_VERSION 2 CACHE STRING "Game version: 0 D44J01, 1 GZLJ01, 2 GZLE01, 3 GZLP01")
set(TWW_GAME_ID GZLE01 CACHE STRING "Disc ID whose generated asset headers are used")

# Generated asset headers ("assets/...", "res/Object/...") come from the player's disc and live
# under the build directory, never in git (native/README.md, "Asset headers").
set(TWW_ASSETS_DIR "${CMAKE_BINARY_DIR}/assets/${TWW_GAME_ID}" CACHE PATH
        "Directory holding the asset headers generated from the player's disc")

set(TWW_GAME_COMPILE_DEFS
        TARGET_PC=1
        VERSION=${TWW_VERSION}
        NDEBUG=1)

# Same order as configure.py's -i list, minus the MSL/Runtime/MetroTRK directories: the host C
# and C++ libraries replace MSL. native/include/pc/msl holds thin shims for the MSL-only header
# names the game includes (algorithm.h, new.h...).
#
# SDK headers (docs/NATIVE_PORT_PHASE2_3.md, step 2.3, decision D2):
# - decomp: the decomp's own dolphin/ headers under native/tww/include (phase 1).
# - aurora: Aurora's headers are the only SDK headers. native/include/sdk holds forwarders for the
#   SDK header names only TWW has (dolphin/os/OS.h -> <dolphin/os.h> + TWW-only declarations) and
#   comes before Aurora, which wins every name both have, so Aurora's own includes stay
#   consistent. native/tww/include/dolphin must never be reached (check/check_sdk_shadow.sh).
set(TWW_SDK_HEADERS decomp CACHE STRING "SDK headers the game compiles against: decomp or aurora")
set_property(CACHE TWW_SDK_HEADERS PROPERTY STRINGS decomp aurora)
if (TWW_SDK_HEADERS STREQUAL "aurora")
    if (NOT TWW_WITH_AURORA)
        message(FATAL_ERROR "TWW_SDK_HEADERS=aurora needs TWW_WITH_AURORA=ON")
    endif ()
    include(${CMAKE_CURRENT_LIST_DIR}/Aurora.cmake)
    set(TWW_SDK_INCLUDE_DIRS
            ${TWW_NATIVE_ROOT}/include/sdk
            ${aurora_SOURCE_DIR}/include)
    # As Dusklight compiles the game (GameABIConfig.cmake): PSMTX* resolve to aurora_mtx's C_MTX*.
    list(APPEND TWW_GAME_COMPILE_DEFS MTX_USE_PS=1)
elseif (TWW_SDK_HEADERS STREQUAL "decomp")
    set(TWW_SDK_INCLUDE_DIRS)
else ()
    message(FATAL_ERROR "TWW_SDK_HEADERS must be decomp or aurora (got '${TWW_SDK_HEADERS}')")
endif ()

set(TWW_GAME_INCLUDE_DIRS
        ${TWW_NATIVE_ROOT}/include
        ${TWW_SDK_INCLUDE_DIRS}
        ${TWW_ROOT}/include
        ${TWW_ASSETS_DIR}/include
        ${TWW_ASSETS_DIR}
        ${TWW_ROOT}/src
        ${TWW_NATIVE_ROOT}/include/pc/msl)

set(TWW_PC_CONFIG_HEADER ${TWW_NATIVE_ROOT}/include/pc/tww_pc_config.h)

set(TWW_GAME_COMPILE_OPTIONS
        # Force-included first in every unit: what Metrowerks and MSL provided implicitly.
        "SHELL:-include ${TWW_PC_CONFIG_HEADER}"
        # Match the GameCube (and x86): plain char is signed. Same as Dusklight on ARM.
        -fsigned-char
        # MWCC was invoked with -Cpp_exceptions off and -RTTI off.
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        # Diagnostics only (no code change). Same set as Dusklight, plus the MWCC-isms clang
        # rejects by default but can accept with identical meaning.
        -Wno-multichar                       # 'ABCD' constants: identical big-endian encoding
        -Wno-unknown-pragmas                 # #pragma optimization_level, scheduling, ...
        -Wno-deprecated-declarations
        -Wno-declaration-after-statement
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-trigraphs>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-non-pod-varargs>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>           # 'register' storage class (C++17)
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-c++11-narrowing>    # implicit narrowing in braces, as MWCC
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-invalid-offsetof>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-volatile>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-enum-conversion>
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-enum-float-conversion>
        -Wno-parentheses
        -Wno-shift-op-parentheses
        -Wno-logical-op-parentheses
        -Wno-bitwise-op-parentheses
        -Wno-dangling-else
        -Wno-unused-value
        -Wno-tautological-compare
        -Wno-tautological-constant-out-of-range-compare
        -Wno-pointer-sign
        -Wno-ignored-attributes
        -Wno-writable-strings
        # 64-bit diagnostics (int-to-pointer-cast, ...) stay visible on purpose: phase 4 input.
        -ferror-limit=50)

if (TWW_SDK_HEADERS STREQUAL "aurora")
    # TWW's dolphin/types.h reached every unit through global.h; its names Aurora lacks (uint,
    # READU32_BE, FLOAT_MIN/MAX) come from this header instead, right after the PC config header.
    list(APPEND TWW_GAME_COMPILE_OPTIONS
            "SHELL:-include ${TWW_NATIVE_ROOT}/include/sdk/tww_sdk_extras.h")
endif ()

add_library(tww_game_headers INTERFACE)
target_compile_definitions(tww_game_headers INTERFACE ${TWW_GAME_COMPILE_DEFS})
target_include_directories(tww_game_headers INTERFACE ${TWW_GAME_INCLUDE_DIRS})
target_compile_options(tww_game_headers INTERFACE ${TWW_GAME_COMPILE_OPTIONS})

if (NOT EXISTS "${TWW_ASSETS_DIR}")
    message(STATUS "tww_native: asset headers not generated yet (${TWW_ASSETS_DIR}); "
            "units that include assets/ or res/Object/ headers will not compile until they are")
endif ()
