# Switch NRO of the BlueWake host, headless milestone: the shared host and
# GXRuntime without Aurora, the donor DSP, and the statically linked game
# composite (cmake/composite with COMPOSITE_STATIC). Configure with the
# devkitPro toolchain; scripts/switch/build_host.sh does it in the container.
cmake_minimum_required(VERSION 3.20)
project(BlueWakeSwitchHost LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
  message(FATAL_ERROR "Configure with the devkitPro Switch toolchain")
endif()

set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(RECOMPCORE_DIR "${REPO_ROOT}/ref/recompcore")
set(GXRUNTIME_DIR "${RECOMPCORE_DIR}/GXRuntime")
set(HOST_SRC "${REPO_ROOT}/runtime/host/src")
set(BLUEWAKE_COMPOSITE_OBJECT "" CACHE FILEPATH
    "gGZLE01_recomp.o from cmake/composite built with COMPOSITE_STATIC=ON")
if(NOT EXISTS "${BLUEWAKE_COMPOSITE_OBJECT}")
  message(FATAL_ERROR "Set BLUEWAKE_COMPOSITE_OBJECT to the static composite object")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)

# GXRuntime alone: the headless renderer, no Aurora, no tests.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(GXRUNTIME_ENABLE_AURORA OFF CACHE BOOL "" FORCE)
set(GXRUNTIME_ENABLE_AURORA_RECOMP OFF CACHE BOOL "" FORCE)
add_subdirectory("${GXRUNTIME_DIR}" gxruntime EXCLUDE_FROM_ALL)

set(PORTLIBS_INCLUDE "${DEVKITPRO}/portlibs/switch/include")
set(SWITCH_COMPAT "${CMAKE_CURRENT_SOURCE_DIR}/compat")

# The donor DSP (Dolphin's DSP LLE from RecompCore), as the iOS build compiles
# it: its own sources plus the small Common shim instead of libcommon.
add_library(fmt STATIC
  "${RECOMPCORE_DIR}/Externals/fmt/fmt/src/format.cc"
  "${RECOMPCORE_DIR}/Externals/fmt/fmt/src/os.cc")
target_include_directories(fmt PUBLIC "${RECOMPCORE_DIR}/Externals/fmt/fmt/include")
target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)
add_library(xxhash STATIC "${RECOMPCORE_DIR}/Externals/xxhash/xxHash/xxhash.c")
target_include_directories(xxhash PUBLIC "${RECOMPCORE_DIR}/Externals/xxhash/xxHash")

set(DSP "${RECOMPCORE_DIR}/Source/Core/Core/DSP")
set(DSPHLE "${RECOMPCORE_DIR}/Source/Core/Core/HW/DSPHLE")
add_library(bluewake_dsp_donor STATIC
  ${DSP}/DSPAccelerator.cpp ${DSP}/DSPAnalyzer.cpp ${DSP}/DSPAssembler.cpp
  ${DSP}/DSPCaptureLogger.cpp ${DSP}/DSPCodeUtil.cpp ${DSP}/DSPCore.cpp
  ${DSP}/DSPDisassembler.cpp ${DSP}/DSPHWInterface.cpp ${DSP}/DSPMemoryMap.cpp
  ${DSP}/DSPStacks.cpp ${DSP}/DSPTables.cpp ${DSP}/LabelMap.cpp
  ${DSP}/Interpreter/DSPIntArithmetic.cpp ${DSP}/Interpreter/DSPIntBranch.cpp
  ${DSP}/Interpreter/DSPIntExtOps.cpp ${DSP}/Interpreter/DSPIntLoadStore.cpp
  ${DSP}/Interpreter/DSPIntMisc.cpp ${DSP}/Interpreter/DSPIntMultiplier.cpp
  ${DSP}/Interpreter/DSPIntTables.cpp ${DSP}/Interpreter/DSPInterpreter.cpp
  ${DSP}/Jit/DSPEmitterBase.cpp
  ${DSPHLE}/DSPHLE.cpp ${DSPHLE}/MailHandler.cpp
  ${DSPHLE}/UCodes/AESnd.cpp ${DSPHLE}/UCodes/ASnd.cpp ${DSPHLE}/UCodes/AX.cpp
  ${DSPHLE}/UCodes/AXWii.cpp ${DSPHLE}/UCodes/CARD.cpp ${DSPHLE}/UCodes/GBA.cpp
  ${DSPHLE}/UCodes/INIT.cpp ${DSPHLE}/UCodes/ROM.cpp ${DSPHLE}/UCodes/UCodes.cpp
  ${DSPHLE}/UCodes/Zelda.cpp ${DSPHLE}/UCodes/ZeldaUCodesTable.cpp
  "${REPO_ROOT}/apple/ios/src/dsp_common_shim.cpp")
target_include_directories(bluewake_dsp_donor PUBLIC
  "${RECOMPCORE_DIR}/Source/Core" "${RECOMPCORE_DIR}/Source" "${RECOMPCORE_DIR}/Externals"
  "${REPO_ROOT}/apple/ios/dsp_generated")
target_include_directories(bluewake_dsp_donor PRIVATE "${SWITCH_COMPAT}" "${PORTLIBS_INCLUDE}")
target_compile_definitions(bluewake_dsp_donor PUBLIC
  _ARCH_64=1 _M_ARM_64=1 _DEFAULT_SOURCE __STDC_CONSTANT_MACROS __STDC_LIMIT_MACROS)
target_compile_options(bluewake_dsp_donor PRIVATE -fno-strict-aliasing -w)
target_link_libraries(bluewake_dsp_donor PUBLIC fmt xxhash)

add_library(bluewake_dsp_adapter STATIC
  ${HOST_SRC}/dsp_adapter.cpp ${HOST_SRC}/dsp_adapter_c.cpp
  ${HOST_SRC}/dsp_adapter_donor_stubs.cpp ${HOST_SRC}/dsp_hle_backend.cpp)
target_link_libraries(bluewake_dsp_adapter PUBLIC bluewake_dsp_donor gxruntime)

# The host: runtime/host/src without the SDL extras, which
# source/headless_stubs.c replaces, and main() renamed so source/switch_main.c
# can set up the SD card and logs first.
add_executable(bluewake_switch_host
  ${HOST_SRC}/main.c ${HOST_SRC}/aram_dma.c ${HOST_SRC}/audio_capture.c
  ${HOST_SRC}/ipl_sram.c ${HOST_SRC}/card_runtime.c ${HOST_SRC}/callback_delivery.c
  ${HOST_SRC}/cycle_domain.c ${HOST_SRC}/delivery_digest.c ${HOST_SRC}/edge_intercepts.c
  ${HOST_SRC}/game_options.c ${HOST_SRC}/composite_provider.c ${HOST_SRC}/fast_load.c
  ${HOST_SRC}/fps_watch.c ${HOST_SRC}/climb.c ${HOST_SRC}/quick_doors.c
  ${HOST_SRC}/draw_tags.c ${HOST_SRC}/return_census.c ${HOST_SRC}/interrupt_sources.c
  ${HOST_SRC}/cold_fallback.c ${HOST_SRC}/external_memory.c ${HOST_SRC}/fpu_context.c
  ${HOST_SRC}/pad_event_schedule.c ${HOST_SRC}/pad_waypoint.c ${HOST_SRC}/pad_wire.c
  ${HOST_SRC}/rel_scratch_allocator.c ${HOST_SRC}/scheduler_contract.c
  ${HOST_SRC}/save_state.c
  source/headless_stubs.c
  source/switch_main.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../source/common/usb_log.c)
set_source_files_properties(${HOST_SRC}/main.c PROPERTIES
  COMPILE_DEFINITIONS "main=bluewake_host_main")
target_include_directories(bluewake_switch_host PRIVATE
  "${HOST_SRC}" "${GXRUNTIME_DIR}/include" "${GXRUNTIME_DIR}/graphics/aurora/include"
  "${RECOMPCORE_DIR}/Source/Core/Core/PowerPC/StaticRecomp"
  "${CMAKE_CURRENT_SOURCE_DIR}/../source/common" "${PORTLIBS_INCLUDE}")
target_compile_definitions(bluewake_switch_host PRIVATE
  DOLRECOMP_CPU_HEADER="core/cpu.h"
  BLUEWAKE_STATIC_COMPOSITE=1
  BLUEWAKE_HAS_DSP_ADAPTER=1
  BLUEWAKE_ENABLE_DEVELOPER_TRACING=0
  BLUEWAKE_EDGE_CENSUS=0)
target_compile_options(bluewake_switch_host PRIVATE
  "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${SWITCH_COMPAT}/switch_compat.h>" -g)
target_link_directories(bluewake_switch_host PRIVATE "${DEVKITPRO}/portlibs/switch/lib")
target_link_libraries(bluewake_switch_host PRIVATE
  "${BLUEWAKE_COMPOSITE_OBJECT}" bluewake_dsp_adapter gxruntime z nx m stdc++)

nx_create_nro(BlueWakeSwitch
  TARGET bluewake_switch_host
  OUTPUT "${CMAKE_BINARY_DIR}/BlueWakeSwitch.nro"
  NOICON
  NONACP)
