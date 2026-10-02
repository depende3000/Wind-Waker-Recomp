# Switch NRO of the BlueWake host: the shared host, GXRuntime, the donor DSP,
# and the statically linked game composite (cmake/composite with
# COMPOSITE_STATIC). By default it is the headless milestone (GXRuntime without
# Aurora); BLUEWAKE_SWITCH_AURORA adds the Aurora renderer (switch/aurora).
# Configure with the devkitPro toolchain; scripts/switch/build_host.sh does it
# in the container.
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

option(BLUEWAKE_SWITCH_AURORA "Build with the Aurora renderer instead of headless only" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

if(BLUEWAKE_SWITCH_AURORA)
  include("${CMAKE_CURRENT_SOURCE_DIR}/../aurora/aurora.cmake")
  set(GXRUNTIME_DIR "${GXRUNTIME_SWITCH_DIR}")
else()
  # GXRuntime alone: the headless renderer.
  set(GXRUNTIME_ENABLE_AURORA OFF CACHE BOOL "" FORCE)
  set(GXRUNTIME_ENABLE_AURORA_RECOMP OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory("${GXRUNTIME_DIR}" gxruntime EXCLUDE_FROM_ALL)
if(BLUEWAKE_SWITCH_AURORA)
  aurora_switch_configure_targets()
endif()

set(PORTLIBS_INCLUDE "${DEVKITPRO}/portlibs/switch/include")
set(SWITCH_COMPAT "${CMAKE_CURRENT_SOURCE_DIR}/compat")

# The donor DSP (Dolphin's DSP LLE from RecompCore), as the iOS build compiles
# it: its own sources plus the small Common shim instead of libcommon.
# With Aurora these come from its extern/ (as on iOS); one fmt per binary.
if(NOT TARGET fmt::fmt)
  add_library(fmt STATIC
    "${RECOMPCORE_DIR}/Externals/fmt/fmt/src/format.cc"
    "${RECOMPCORE_DIR}/Externals/fmt/fmt/src/os.cc")
  target_include_directories(fmt PUBLIC "${RECOMPCORE_DIR}/Externals/fmt/fmt/include")
  target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)
  add_library(fmt::fmt ALIAS fmt)
endif()
if(NOT TARGET xxhash)
  add_library(xxhash STATIC "${RECOMPCORE_DIR}/Externals/xxhash/xxHash/xxhash.c")
  target_include_directories(xxhash PUBLIC "${RECOMPCORE_DIR}/Externals/xxhash/xxHash")
endif()

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
target_link_libraries(bluewake_dsp_donor PUBLIC fmt::fmt xxhash)

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
  source/thread_stack.c
  $<$<NOT:$<BOOL:${BLUEWAKE_SWITCH_AURORA}>>:${CMAKE_CURRENT_SOURCE_DIR}/source/aurora_stubs.c>
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
# Every thread gets a stack Dawn's shader compiler fits in (source/thread_stack.c).
target_link_options(bluewake_switch_host PRIVATE "LINKER:--wrap=pthread_create")
target_link_libraries(bluewake_switch_host PRIVATE
  "${BLUEWAKE_COMPOSITE_OBJECT}" bluewake_dsp_adapter gxruntime z nx m stdc++)
if(BLUEWAKE_SWITCH_AURORA)
  target_compile_definitions(bluewake_switch_host PRIVATE BLUEWAKE_SWITCH_AURORA=1)
  target_link_libraries(bluewake_switch_host PRIVATE GXRuntime::aurora)

  # Aurora without the game: window, device, present, ImGui, PAD and audio
  # on the console (switch/aurora/smoke).
  add_executable(aurora_smoke
    ${CMAKE_CURRENT_SOURCE_DIR}/../aurora/smoke/aurora_smoke.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../source/common/usb_log.c
    source/thread_stack.c)
  target_include_directories(aurora_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../source/common"
                             "${PORTLIBS_INCLUDE}")
  target_link_options(aurora_smoke PRIVATE "LINKER:--wrap=pthread_create")
  target_link_libraries(aurora_smoke PRIVATE
    aurora::core aurora::gx aurora::pad aurora::si aurora::os aurora::vi SDL3::SDL3-static sqlite3 nx)
  get_target_property(SMOKE_SQLITE_SOURCES sqlite3 SOURCES)
  list(GET SMOKE_SQLITE_SOURCES 0 SMOKE_SQLITE_AMALGAMATION)
  get_filename_component(SMOKE_SQLITE_DIR "${SMOKE_SQLITE_AMALGAMATION}" DIRECTORY)
  target_include_directories(aurora_smoke PRIVATE "${SMOKE_SQLITE_DIR}")
  nx_generate_nacp(aurora_smoke.nacp
    NAME "Wind Waker Recomp - Aurora test" AUTHOR "Wind Waker Recomp contributors" VERSION "0.1.0")
  nx_create_nro(BlueWakeAuroraSmoke
    TARGET aurora_smoke
    OUTPUT "${CMAKE_BINARY_DIR}/BlueWakeAuroraSmoke.nro"
    NACP aurora_smoke.nacp
    NOICON)
endif()

# The name, author and version the Homebrew Menu shows.
# The two builds have different file names so both can sit on the SD card.
if(BLUEWAKE_SWITCH_AURORA)
  set(BLUEWAKE_SWITCH_TITLE "Wind Waker Recomp")
  set(BLUEWAKE_SWITCH_NRO BlueWakeSwitchAurora)
else()
  set(BLUEWAKE_SWITCH_TITLE "Wind Waker Recomp (headless)")
  set(BLUEWAKE_SWITCH_NRO BlueWakeSwitch)
endif()
nx_generate_nacp(bluewake_switch.nacp
  NAME "${BLUEWAKE_SWITCH_TITLE}" AUTHOR "Wind Waker Recomp contributors" VERSION "0.1.0")
nx_create_nro(${BLUEWAKE_SWITCH_NRO}
  TARGET bluewake_switch_host
  OUTPUT "${CMAKE_BINARY_DIR}/${BLUEWAKE_SWITCH_NRO}.nro"
  NACP bluewake_switch.nacp
  NOICON)
