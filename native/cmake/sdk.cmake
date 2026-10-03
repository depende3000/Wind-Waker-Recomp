# tww_sdk: the TWW-specific part of the GameCube SDK, on top of Aurora's SDK libraries
# (docs/NATIVE_PORT_PHASE2_3.md, step 2.2, decisions D2 and D7).
#
# - Sources are globbed from native/sdk/src/**/*.{c,cpp} with CONFIGURE_DEPENDS, so the phase 2
#   steps that add SDK code (2.6a-2.6f) only add files and never touch CMake.
# - Compiled against Aurora's headers only: never the game's headers or flags (tww_game_headers).
# - tww_sdk_smoke is a headless test program (no window, no GPU). Its tests are globbed from
#   native/sdk/tests/*.cpp and register themselves by name (see tests/smoke.h).
#
# Needs Aurora, so it is only built with TWW_WITH_AURORA=ON.

if (NOT TWW_WITH_AURORA)
    return()
endif ()

set(TWW_SDK_ROOT "${CMAKE_CURRENT_LIST_DIR}/../sdk")
cmake_path(NORMAL_PATH TWW_SDK_ROOT)

file(GLOB_RECURSE TWW_SDK_SOURCES CONFIGURE_DEPENDS
        "${TWW_SDK_ROOT}/src/*.c"
        "${TWW_SDK_ROOT}/src/*.cpp")

add_library(tww_sdk STATIC ${TWW_SDK_SOURCES})
target_include_directories(tww_sdk PUBLIC "${TWW_SDK_ROOT}/include")
# As the game is compiled in Dusklight (GameABIConfig.cmake): the PSMTX* names resolve to the
# C_MTX* functions that aurora_mtx exports. TARGET_PC and AURORA come from aurora::core.
target_compile_definitions(tww_sdk PUBLIC MTX_USE_PS=1)
target_compile_definitions(tww_sdk PRIVATE "TWW_AURORA_COMMIT_STR=\"${TWW_AURORA_COMMIT}\"")
target_link_libraries(tww_sdk PUBLIC ${TWW_AURORA_LIBS})
# The DSP behind src/audio/DSP.cpp: Dolphin's DSPHLE (cmake/dsp_hle.cmake).
target_link_libraries(tww_sdk PRIVATE tww_dsp_hle)

file(GLOB TWW_SDK_SMOKE_SOURCES CONFIGURE_DEPENDS
        "${TWW_SDK_ROOT}/tests/*.cpp")
add_executable(tww_sdk_smoke ${TWW_SDK_SMOKE_SOURCES})
target_link_libraries(tww_sdk_smoke PRIVATE tww_sdk)
# The program sits at the top of the build directory: build/native-mac/tww_sdk_smoke.
set_target_properties(tww_sdk_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# tww_sdk_smoke_tsan: the same tests and tww_sdk sources built with ThreadSanitizer (step 2.6a:
# the OS thread, mutex, message and alarm code must run race-free). Aurora's libraries are linked
# uninstrumented, without aurora::dvd: it pulls in nod (Rust), and with it the TSan link on macOS
# fails ("too many personality routines for compact unwind": C, C++, Objective-C and Rust). The
# tests do not use DVD, except the DTK part of the "audio" test (step 2.6f): src/audio/DTK.cpp
# drives Aurora's DVD stream commands, so it is left out here and TWW_SDK_SMOKE_NO_DVD skips that
# part. Not part of `all`:
#   ninja tww_sdk_smoke_tsan && build/native-mac/tww_sdk_smoke_tsan
# On macOS 26.6 Xcode's clang 17 TSan runtime crashes at start-up; native/sdk/README.md
# ("ThreadSanitizer run") builds it in build/native-mac-tsan with the Command Line Tools clang.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT CMAKE_CROSSCOMPILING)
    set(TWW_SDK_TSAN_SOURCES ${TWW_SDK_SOURCES})
    list(FILTER TWW_SDK_TSAN_SOURCES EXCLUDE REGEX "/src/audio/DTK\\.cpp$")
    add_executable(tww_sdk_smoke_tsan EXCLUDE_FROM_ALL ${TWW_SDK_TSAN_SOURCES} ${TWW_SDK_SMOKE_SOURCES})
    target_include_directories(tww_sdk_smoke_tsan PRIVATE "${TWW_SDK_ROOT}/include")
    target_compile_definitions(tww_sdk_smoke_tsan PRIVATE MTX_USE_PS=1 TWW_SDK_SMOKE_NO_DVD=1
            "TWW_AURORA_COMMIT_STR=\"${TWW_AURORA_COMMIT}\"")
    target_compile_options(tww_sdk_smoke_tsan PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
    target_link_options(tww_sdk_smoke_tsan PRIVATE -fsanitize=thread)
    set(TWW_SDK_TSAN_LIBS ${TWW_AURORA_LIBS})
    list(REMOVE_ITEM TWW_SDK_TSAN_LIBS aurora::dvd)
    target_link_libraries(tww_sdk_smoke_tsan PRIVATE ${TWW_SDK_TSAN_LIBS} tww_dsp_hle)
    set_target_properties(tww_sdk_smoke_tsan PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif ()
