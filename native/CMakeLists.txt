# Native port of The Wind Waker: the decompiled game (native/tww) built with the host compiler.
#
# Phase 1 (docs/NATIVE_PORT_PLAN.md): every translation unit of the game compiles to an object
# with clang under TARGET_PC, against the host C/C++ library instead of Metrowerks' MSL.
# Nothing is linked yet: the GameCube SDK over Aurora is phase 2, the RELs linked statically
# phase 3, 64-bit and endianness semantics phase 4, JAudio phase 5.
#
# Configure:  cmake -S native -B build/native-mac -G Ninja
# Build:      ninja -C build/native-mac -k 0 <module>     (see native/README.md)

cmake_minimum_required(VERSION 3.25)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING
            "Build type options: Debug Release RelWithDebInfo MinSizeRel" FORCE)
endif ()

if (APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architecture" FORCE)
endif ()

project(tww_native LANGUAGES C CXX)

if (APPLE AND NOT CMAKE_OSX_SYSROOT)
    # If the Xcode SDK is lagging behind the system version, CMake needs this done first
    execute_process(COMMAND xcrun --sdk macosx --show-sdk-path
            OUTPUT_VARIABLE CMAKE_OSX_SYSROOT
            OUTPUT_STRIP_TRAILING_WHITESPACE)
endif ()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "tww_native: only clang is tested (got ${CMAKE_CXX_COMPILER_ID})")
endif ()

set(TWW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/tww" CACHE INTERNAL "Decompiled game source")
set(TWW_NATIVE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "Native port root")

# Compile definitions, include paths, flags and the force-included PC config header.
include(cmake/GameConfig.cmake)
# Units excluded from the build until a later phase; every entry has its reason.
include(cmake/deferred.cmake)
# One OBJECT library per module, each behind a TWW_MODULE_<name> option.
include(cmake/modules.cmake)
# Aurora, the GameCube SDK over WebGPU (phase 2), behind TWW_WITH_AURORA.
include(cmake/Aurora.cmake)
# tww_sdk, the TWW-specific SDK over Aurora, and its headless test tww_sdk_smoke (with Aurora only).
include(cmake/sdk.cmake)
# Link census (phase 2): `ninja tww_link_census` lists what the non-REL units still need.
include(cmake/census.cmake)

# Scaffold check: proves the toolchain, the include paths and the PC config header work.
add_library(tww_scaffold_check OBJECT
        check/pc_check.cpp
        check/pc_check.c)
target_link_libraries(tww_scaffold_check PRIVATE tww_game_headers)

# SDK header check (phase 2, step 2.3): every SDK header name the decomp has, in one unit, against
# the configured SDK headers (TWW_SDK_HEADERS). In aurora mode, tww_sdk_shadow_check also proves
# that none of them resolves under native/tww/include/dolphin.
add_library(tww_sdk_header_check OBJECT check/sdk_headers.cpp)
target_link_libraries(tww_sdk_header_check PRIVATE tww_game_headers)
if (TWW_SDK_HEADERS STREQUAL "aurora")
    target_compile_definitions(tww_sdk_header_check PRIVATE TWW_SDK_HEADERS_AURORA=1)
    add_custom_target(tww_sdk_shadow_check
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/check/check_sdk_shadow.sh" "${CMAKE_BINARY_DIR}"
            DEPENDS tww_sdk_header_check
            USES_TERMINAL
            COMMENT "Checking that no SDK header resolves under native/tww/include/dolphin")
endif ()
