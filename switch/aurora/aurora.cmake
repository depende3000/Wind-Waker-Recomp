# Aurora (GXRuntime's GX renderer) for the Switch. Included by switch/host
# when BLUEWAKE_SWITCH_AURORA is on. Provides, before GXRuntime is added:
#   - Dawn for the OpenGL ES backend (switch/dawn/dawn.cmake);
#   - SDL3-static: SDL 3's public headers with sdl3_shim/, a small libnx
#     implementation of the SDL 3 functions Aurora calls (Aurora only reaches
#     SDL; there is no public SDL 3 port for Horizon);
#   - zlib and libpng from devkitPro's portlibs (Aurora finds the FreeType
#     and zstd CMake packages there itself).
# Then adds a copy of GXRuntime with patches/*.patch applied, so ref/ stays
# the pinned checkout. Sets GXRUNTIME_SWITCH_DIR to that copy.

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/../dawn/dawn.cmake")

# SDL 3's public headers, at the version Aurora pins (AURORA_SDL3_REF).
# SOURCE_SUBDIR points nowhere so SDL's own build is never added.
FetchContent_Declare(sdl3_headers
  URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.10.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR do-not-build)
FetchContent_MakeAvailable(sdl3_headers)

file(GLOB SDL3_SHIM_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/sdl3_shim/*.c")
add_library(SDL3-static STATIC ${SDL3_SHIM_SOURCES})
add_library(SDL3::SDL3-static ALIAS SDL3-static)
target_include_directories(SDL3-static PUBLIC "${sdl3_headers_SOURCE_DIR}/include")
target_link_libraries(SDL3-static PUBLIC nx)

# devkitPro portlibs, under the target names Aurora's extern/ looks for.
set(SWITCH_PORTLIBS "${DEVKITPRO_ROOT}/portlibs/switch")
function(switch_portlib target)
  add_library(${target} INTERFACE IMPORTED GLOBAL)
  target_include_directories(${target} INTERFACE "${SWITCH_PORTLIBS}/include")
  target_link_directories(${target} INTERFACE "${SWITCH_PORTLIBS}/lib")
endfunction()
switch_portlib(ZLIB::ZLIB)
target_link_libraries(ZLIB::ZLIB INTERFACE z)
switch_portlib(PNG::PNG)
target_link_libraries(PNG::PNG INTERFACE png16 z)

# Tracy, Aurora's profiler, stays off; Aurora only needs its headers (whose
# macros are empty without TRACY_ENABLE), and its client does not build for
# Horizon. Same archive Aurora's extern/ pins.
FetchContent_Declare(tracy
  URL https://github.com/wolfpld/tracy/archive/6789e7d6f9a65ec98926b602097a33a9676d2606.tar.gz
  URL_HASH SHA256=ebfe4fb50d7c254901979355c80a7d4cd33624aa2ec0fe90b3b238153cd5d69b
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE
  SOURCE_SUBDIR do-not-build)
FetchContent_MakeAvailable(tracy)
add_library(TracyClient INTERFACE)
target_include_directories(TracyClient INTERFACE "${tracy_SOURCE_DIR}/public")

# A patched copy of GXRuntime, refreshed whenever the patch set changes.
set(GXRUNTIME_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../../ref/recompcore/GXRuntime")
set(GXRUNTIME_SWITCH_DIR "${CMAKE_BINARY_DIR}/gxruntime-switch")
file(GLOB AURORA_SWITCH_PATCHES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/patches/*.patch")
list(SORT AURORA_SWITCH_PATCHES)
set(AURORA_SWITCH_STAMP "")
foreach(PATCH IN LISTS AURORA_SWITCH_PATCHES)
  file(SHA256 "${PATCH}" PATCH_HASH)
  string(APPEND AURORA_SWITCH_STAMP "${PATCH_HASH}\n")
endforeach()
set(STAMP_FILE "${GXRUNTIME_SWITCH_DIR}.stamp")
set(PREVIOUS_STAMP "")
if(EXISTS "${STAMP_FILE}")
  file(READ "${STAMP_FILE}" PREVIOUS_STAMP)
endif()
if(NOT EXISTS "${GXRUNTIME_SWITCH_DIR}" OR NOT PREVIOUS_STAMP STREQUAL AURORA_SWITCH_STAMP)
  file(REMOVE_RECURSE "${GXRUNTIME_SWITCH_DIR}")
  file(COPY "${GXRUNTIME_SOURCE}/" DESTINATION "${GXRUNTIME_SWITCH_DIR}")
  find_program(PATCH_EXECUTABLE patch REQUIRED)
  foreach(PATCH IN LISTS AURORA_SWITCH_PATCHES)
    execute_process(
      COMMAND "${PATCH_EXECUTABLE}" -p1 -i "${PATCH}"
      WORKING_DIRECTORY "${GXRUNTIME_SWITCH_DIR}"
      RESULT_VARIABLE PATCH_RESULT
      OUTPUT_VARIABLE PATCH_OUTPUT
      ERROR_VARIABLE PATCH_OUTPUT)
    if(NOT PATCH_RESULT EQUAL 0)
      message(FATAL_ERROR "Could not apply ${PATCH}:\n${PATCH_OUTPUT}")
    endif()
  endforeach()
  file(WRITE "${STAMP_FILE}" "${AURORA_SWITCH_STAMP}")
  list(LENGTH AURORA_SWITCH_PATCHES PATCH_COUNT)
  message(STATUS "GXRuntime copied for the Switch with ${PATCH_COUNT} patches")
endif()

# Aurora's providers find the targets above and use them as they are.
set(AURORA_SDL3_PROVIDER vendor CACHE STRING "" FORCE)
set(AURORA_DAWN_PROVIDER vendor CACHE STRING "" FORCE)
set(AURORA_ENABLE_CARD OFF CACHE BOOL "" FORCE)
set(AURORA_ENABLE_DVD OFF CACHE BOOL "" FORCE)
set(GXRUNTIME_ENABLE_AURORA ON CACHE BOOL "" FORCE)
set(GXRUNTIME_ENABLE_AURORA_RECOMP ON CACHE BOOL "" FORCE)

# Call after GXRuntime has been added: settings for targets Aurora's extern/
# creates. sqlite (Aurora's pipeline cache) without WAL or memory-mapped
# files, which need mmap.
function(aurora_switch_configure_targets)
  # ImGui's default "open in shell" uses fork/exec/waitpid.
  if(TARGET imgui)
    target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS)
  endif()
  if(TARGET sqlite3)
    target_compile_definitions(sqlite3 PRIVATE SQLITE_OMIT_WAL=1 SQLITE_MAX_MMAP_SIZE=0
                               SQLITE_OMIT_LOAD_EXTENSION=1 SQLITE_THREADSAFE=1)
    target_sources(sqlite3 PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/sqlite_horizon.c")
  endif()
endfunction()
