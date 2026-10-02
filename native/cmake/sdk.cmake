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

file(GLOB TWW_SDK_SMOKE_SOURCES CONFIGURE_DEPENDS
        "${TWW_SDK_ROOT}/tests/*.cpp")
add_executable(tww_sdk_smoke ${TWW_SDK_SMOKE_SOURCES})
target_link_libraries(tww_sdk_smoke PRIVATE tww_sdk)
# The program sits at the top of the build directory: build/native-mac/tww_sdk_smoke.
set_target_properties(tww_sdk_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
