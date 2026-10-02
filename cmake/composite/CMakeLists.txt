cmake_minimum_required(VERSION 3.16)
project(bluewake_composite C)

set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(GAME_ID "GZLE01" CACHE STRING "Six-character disc ID")
set(COMPOSITE_DIR "" CACHE PATH "Composite output dir (generated.h, module_tables.inc, rel_modules.inc)")
set(GXRUNTIME_DIR "" CACHE PATH "GXRuntime checkout")
set(ABI_DIR "" CACHE PATH "Directory containing StaticRecompABI.h")
set(COMPOSITE_OPTIMIZATION_LEVEL "2" CACHE STRING "Generated composite optimization level (0, 1, or 2)")
set_property(CACHE COMPOSITE_OPTIMIZATION_LEVEL PROPERTY STRINGS 0 1 2)
if(NOT COMPOSITE_OPTIMIZATION_LEVEL MATCHES "^[012]$")
  message(FATAL_ERROR "COMPOSITE_OPTIMIZATION_LEVEL must be 0, 1, or 2")
endif()
option(COMPOSITE_STATIC
  "Build one relocatable object with bwc_-prefixed symbols for a statically linked host (the Switch NRO) instead of a shared library"
  OFF)
option(COMPOSITE_STATIC_LINK
  "With COMPOSITE_STATIC, also link the objects into that object here (off when the compiler cannot, as with switch/composite/clang-switch.cmake)"
  ON)
set(COMPOSITE_O1_FALLBACK_SOURCES
  "chunks_d_a_npc_so_148/chunk_0000_rel1_C10800F4.c;chunks_dol/chunk_0140_text1_802316E0.c;chunks_dol/chunk_0202_text1_803296E0.c"
  CACHE STRING "Composite-relative generated sources compiled at O1 when the target uses O2")

foreach(REQUIRED_FILE generated.h generated_composite.h module_tables.inc rel_modules.inc rel_data.inc)
  if(NOT EXISTS "${COMPOSITE_DIR}/${REQUIRED_FILE}")
    message(FATAL_ERROR "Missing ${REQUIRED_FILE} in COMPOSITE_DIR=${COMPOSITE_DIR}")
  endif()
endforeach()
if(NOT EXISTS "${GXRUNTIME_DIR}")
  message(FATAL_ERROR "GXRuntime not found at ${GXRUNTIME_DIR}")
endif()
if(NOT EXISTS "${ABI_DIR}/StaticRecompABI.h")
  message(FATAL_ERROR "StaticRecompABI.h not found in ${ABI_DIR}")
endif()

set(MODULE_NAME "g${GAME_ID}_recomp")

# Collect the DOL's C or native-object chunks plus C REL chunks.
file(GLOB DOL_CHUNK_SOURCES "${COMPOSITE_DIR}/chunks_dol/*.c")
file(GLOB DOL_CHUNK_OBJECTS "${COMPOSITE_DIR}/chunks_dol/*.o")
if(DOL_CHUNK_SOURCES AND DOL_CHUNK_OBJECTS)
  message(FATAL_ERROR "DOL composite contains both C and native object chunks")
endif()
if(NOT DOL_CHUNK_SOURCES AND NOT DOL_CHUNK_OBJECTS)
  message(FATAL_ERROR "DOL composite contains no translated chunks")
endif()
set_source_files_properties(${DOL_CHUNK_OBJECTS}
  PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
file(GLOB REL_CHUNK_GLOBS LIST_DIRECTORIES TRUE "${COMPOSITE_DIR}/chunks_*")
set(CHUNK_INPUTS "")
foreach(CHUNK_DIR ${REL_CHUNK_GLOBS})
  if(IS_DIRECTORY "${CHUNK_DIR}" AND NOT CHUNK_DIR MATCHES "chunks_dol")
    file(GLOB RC "${CHUNK_DIR}/*.c")
    list(APPEND CHUNK_INPUTS ${RC})
  endif()
endforeach()
list(APPEND CHUNK_INPUTS ${DOL_CHUNK_SOURCES})
list(APPEND CHUNK_INPUTS ${DOL_CHUNK_OBJECTS})
list(LENGTH CHUNK_INPUTS NUM_CHUNKS)
message(STATUS "${NUM_CHUNKS} chunk translation units")

if(COMPOSITE_STATIC)
  # External DOL objects join at the ld -r step below.
  set(COMPOSITE_LIBRARY_TYPE OBJECT)
  set(COMPOSITE_COMPILED_CHUNKS ${CHUNK_INPUTS})
  list(REMOVE_ITEM COMPOSITE_COMPILED_CHUNKS ${DOL_CHUNK_OBJECTS})
else()
  set(COMPOSITE_LIBRARY_TYPE SHARED)
  set(COMPOSITE_COMPILED_CHUNKS ${CHUNK_INPUTS})
endif()

add_library(${MODULE_NAME} ${COMPOSITE_LIBRARY_TYPE}
  module_export.c
  dispatch_loop.c
  ${GXRUNTIME_DIR}/src/core/cpu.c
  ${GXRUNTIME_DIR}/src/core/cpu_exception.c
  ${GXRUNTIME_DIR}/src/core/cpu_interpreter.c
  ${GXRUNTIME_DIR}/src/core/cpu_interpreter_table.c
  ${GXRUNTIME_DIR}/src/core/cpu_interpreter_float.c
  ${GXRUNTIME_DIR}/src/core/cpu_interpreter_integer.c
  ${COMPOSITE_COMPILED_CHUNKS})

set_target_properties(${MODULE_NAME} PROPERTIES PREFIX "" OUTPUT_NAME "${MODULE_NAME}" UNITY_BUILD OFF C_VISIBILITY_PRESET hidden)
target_compile_definitions(${MODULE_NAME} PRIVATE MODULE_GAME_ID="${GAME_ID}" DOLRECOMP_CPU_HEADER="core/cpu.h")
target_include_directories(${MODULE_NAME} PRIVATE "${COMPOSITE_DIR}" "${GXRUNTIME_DIR}/include" "${ABI_DIR}")
if(APPLE)
  set_target_properties(${MODULE_NAME} PROPERTIES SUFFIX ".dylib")
endif()
if(NOT COMPOSITE_STATIC)
  target_link_libraries(${MODULE_NAME} PRIVATE m)
endif()
target_compile_options(${MODULE_NAME} PRIVATE -O${COMPOSITE_OPTIMIZATION_LEVEL} -ffp-contract=off)

if(COMPOSITE_OPTIMIZATION_LEVEL STREQUAL "2")
  foreach(FALLBACK_SOURCE IN LISTS COMPOSITE_O1_FALLBACK_SOURCES)
    set(FALLBACK_PATH "${COMPOSITE_DIR}/${FALLBACK_SOURCE}")
    if(NOT EXISTS "${FALLBACK_PATH}")
      message(FATAL_ERROR "O1 fallback source not found: ${FALLBACK_PATH}")
    endif()
    list(FIND CHUNK_INPUTS "${FALLBACK_PATH}" FALLBACK_INDEX)
    if(FALLBACK_INDEX EQUAL -1)
      message(FATAL_ERROR "O1 fallback is not a generated chunk: ${FALLBACK_PATH}")
    endif()
    set_property(SOURCE "${FALLBACK_PATH}" APPEND PROPERTY COMPILE_OPTIONS -O1)
    message(STATUS "O1 optimizer fallback: ${FALLBACK_SOURCE}")
  endforeach()
endif()

if(COMPOSITE_STATIC)
  # One relocatable object, then every defined global renamed to bwc_<name>:
  # the static counterpart of the shared library's private symbol namespace.
  # The host reaches its exports through runtime/host/src/composite_provider.c.
  # The object list, for this link step or one done elsewhere.
  set(COMPOSITE_RSP "${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}.objects.rsp")
  file(GENERATE OUTPUT "${COMPOSITE_RSP}"
    CONTENT "$<JOIN:$<TARGET_OBJECTS:${MODULE_NAME}>,\n>\n$<JOIN:${DOL_CHUNK_OBJECTS},\n>\n")
endif()

if(COMPOSITE_STATIC AND COMPOSITE_STATIC_LINK)
  find_program(COMPOSITE_NM NAMES ${CMAKE_NM} nm REQUIRED)
  find_program(COMPOSITE_OBJCOPY NAMES ${CMAKE_OBJCOPY} objcopy REQUIRED)
  set(COMPOSITE_COMBINED "${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}.combined.o")
  set(COMPOSITE_STATIC_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}.o")
  add_custom_command(
    OUTPUT "${COMPOSITE_STATIC_OBJECT}"
    COMMAND "${CMAKE_LINKER}" -r -o "${COMPOSITE_COMBINED}" "@${COMPOSITE_RSP}"
    COMMAND "${CMAKE_COMMAND}" -DNM=${COMPOSITE_NM} -DOBJCOPY=${COMPOSITE_OBJCOPY}
            -DPREFIX=bwc_ -DINPUT=${COMPOSITE_COMBINED} -DOUTPUT=${COMPOSITE_STATIC_OBJECT}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/prefix_symbols.cmake"
    DEPENDS ${MODULE_NAME} $<TARGET_OBJECTS:${MODULE_NAME}> ${DOL_CHUNK_OBJECTS}
            "${CMAKE_CURRENT_SOURCE_DIR}/prefix_symbols.cmake"
    COMMAND_EXPAND_LISTS
    VERBATIM)
  add_custom_target(${MODULE_NAME}_static ALL DEPENDS "${COMPOSITE_STATIC_OBJECT}")
endif()
