# The game executable "tww" (docs/NATIVE_PORT_PHASE2_3.md, phase 3, step 3.8).
#
#   ninja -C build/native-mac tww
#
# - One executable from the objects of every enabled module (nothing is recompiled), linked
#   with tww_sdk, the Aurora SDK libraries (TWW_AURORA_LIBS, public on tww_sdk) and aurora::main,
#   which owns the process entry point and calls the game's main (renamed aurora_main by
#   <aurora/main.h> in m_Do_main.cpp).
# - Object order on the link line: the main.dol units first, then the REL units of rel_units.txt
#   (step 3.5 links them statically), then the audio module (JAudio/JAZelAudio, step 3.7). ld64
#   runs static initialisers in input order, so the REL units see main.dol's globals already
#   initialised, as they did on GameCube when a REL's _prolog ran after boot.
# - Strict: no -undefined dynamic_lookup. Every symbol must resolve here, so `nm -u tww` lists
#   only system and framework symbols.
# - Needs Aurora (tww_sdk) and every module; skipped otherwise. Not part of `all`.
include_guard(GLOBAL)

# The run harness (docs/NATIVE_PORT_PHASE4_6.md, step 6.0): native/src/pc/pc_*.cpp, globbed into
# the static library tww_pc (API: native/include/pc/pc_harness.h). Compiled like the game units
# (tww_game_headers), since pc_smoke.cpp reads game state. Game units call into it under TARGET_PC,
# so the link census bundle links it too: its symbols must not show up as unresolved.
# The port helpers' sources (native/src/helpers/*.cpp, step 4.0b: OffsetPtr; headers in
# native/include/helpers) go into the same library, so the game and tww_pc_tests link one copy.
file(GLOB _pc_sources CONFIGURE_DEPENDS
        "${TWW_NATIVE_ROOT}/src/pc/pc_*.cpp"
        "${TWW_NATIVE_ROOT}/src/helpers/*.cpp")
list(SORT _pc_sources)
add_library(tww_pc STATIC ${_pc_sources})
target_link_libraries(tww_pc PRIVATE tww_game_headers)
target_include_directories(tww_pc PRIVATE "${TWW_NATIVE_ROOT}/src/pc")
# pc_main.cpp (step 6.1) sets tww_sdk's thread hooks (tww_sdk/hooks.h); tww itself links tww_sdk.
target_include_directories(tww_pc PRIVATE "${TWW_NATIVE_ROOT}/sdk/include")
if (TARGET tww_link_census)
    target_link_libraries(tww_link_census PRIVATE tww_pc)
endif ()

# tww_pc_tests (step 4.0b): host tests of the port helpers (BE(T), OffsetPtr), compiled like a game
# unit. Headless; prints "ok":  ninja tww_pc_tests && build/native-mac/tww_pc_tests
# tww_sdk supplies the SDK functions the helpers call (OSPanic: its default aborts); c_sxyz.cpp (no
# dependencies) the csXyz constructor that BE<csXyz> converts through.
if (TARGET tww_sdk)
    add_executable(tww_pc_tests "${TWW_NATIVE_ROOT}/check/pc_tests.cpp"
            "${TWW_ROOT}/src/SSystem/SComponent/c_sxyz.cpp")
    target_link_libraries(tww_pc_tests PRIVATE tww_game_headers tww_pc tww_sdk)
    set_target_properties(tww_pc_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif ()

if (NOT TARGET tww_sdk)
    message(STATUS "tww_native: executable tww disabled (needs TWW_WITH_AURORA=ON, i.e. tww_sdk)")
    return()
endif ()

set(_exe_missing)
foreach (_m IN LISTS TWW_MODULES)
    if (NOT TARGET ${_m})
        list(APPEND _exe_missing ${_m})
    endif ()
endforeach ()
if (_exe_missing)
    message(STATUS "tww_native: executable tww disabled (modules not enabled: ${_exe_missing})")
    return()
endif ()

# REL unit paths (relative to native/tww/src), as census.cmake reads them.
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/rel_units.txt" _exe_rel_lines)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/rel_units.txt")
set(_exe_rel_units)
foreach (_l IN LISTS _exe_rel_lines)
    string(STRIP "${_l}" _l)
    if (_l AND NOT _l MATCHES "^#")
        list(APPEND _exe_rel_units "${_l}")
    endif ()
endforeach ()

# Per module: its main.dol objects and its REL objects, split with $<FILTER> over
# $<TARGET_OBJECTS> (chunks of 40 REL names per regex, to keep each regex small).
set(_exe_dol)
set(_exe_rel)
set(_exe_audio)
set(_exe_n_rel 0)
foreach (_m IN LISTS TWW_MODULES)
    if (_m STREQUAL "audio")
        list(APPEND _exe_audio "$<TARGET_OBJECTS:${_m}>")
        continue()
    endif ()
    get_property(_srcs GLOBAL PROPERTY "TWW_MODULE_SOURCES:${_m}")
    set(_names)
    foreach (_s IN LISTS _srcs)
        file(RELATIVE_PATH _rel "${TWW_ROOT}/src" "${_s}")
        if (_rel IN_LIST _exe_rel_units)
            string(REGEX REPLACE "[.](c|cpp)$" "" _stem "${_rel}")
            list(APPEND _names "${_stem}")
        endif ()
    endforeach ()
    list(LENGTH _names _n_names)
    math(EXPR _exe_n_rel "${_exe_n_rel} + ${_n_names}")

    set(_keep "$<TARGET_OBJECTS:${_m}>")
    set(_begin 0)
    while (_begin LESS _n_names)
        list(SUBLIST _names ${_begin} 40 _chunk)
        math(EXPR _begin "${_begin} + 40")
        list(JOIN _chunk "|" _alt)
        set(_rx "/tww/src/(${_alt})[.](c|cpp)[.]o$")
        set(_keep "$<FILTER:${_keep},EXCLUDE,${_rx}>")
        list(APPEND _exe_rel "$<FILTER:$<TARGET_OBJECTS:${_m}>,INCLUDE,${_rx}>")
    endwhile ()
    list(APPEND _exe_dol "${_keep}")
endforeach ()

# The objects go in through a response file, in the order above (main.dol, REL, audio); the
# executable's only source is an empty generated unit.
set(_exe_dir "${CMAKE_BINARY_DIR}/tww_exe")
set(_exe_rsp "${_exe_dir}/objects.rsp")
file(GENERATE OUTPUT "${_exe_rsp}"
        CONTENT "$<JOIN:${_exe_dol},\n>\n$<JOIN:${_exe_rel},\n>\n$<JOIN:${_exe_audio},\n>\n")
file(GENERATE OUTPUT "${_exe_dir}/tww_exe_stub.c" CONTENT
        "/* Generated by native/cmake/executable.cmake: the tww executable's only source; the game's\n   objects come in through objects.rsp. */\nint tww_exe_stub;\n")

set(_exe_objects)
foreach (_m IN LISTS TWW_MODULES)
    list(APPEND _exe_objects "$<TARGET_OBJECTS:${_m}>")
endforeach ()

add_executable(tww EXCLUDE_FROM_ALL "${_exe_dir}/tww_exe_stub.c")
add_dependencies(tww ${TWW_MODULES})
target_link_options(tww PRIVATE "@${_exe_rsp}")
target_link_libraries(tww PRIVATE tww_pc tww_sdk aurora::main)
set_target_properties(tww PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        # The game is C++: link with the C++ driver, so libc++/libc++abi resolve the C++ runtime.
        LINKER_LANGUAGE CXX
        # Relink when an object or the object list changes (they are not sources of the target).
        LINK_DEPENDS "${_exe_rsp};${_exe_objects}")

message(STATUS "tww_native: executable tww from ${_exe_n_rel} REL units after the main.dol "
        "units, then audio")
