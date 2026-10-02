# Game modules: one OBJECT library per module, built and fixed one at a time in phase 1.
#
# Each module is behind the option TWW_MODULE_<name> (with '-' as '_'), OFF until its sources
# compile (modules in TWW_MODULES_READY default to ON); turn one on with
# -DTWW_MODULE_<name>=ON or everything with -DTWW_ALL_MODULES=ON.
# Sources are taken from fixed directories of native/tww/src (sorted, so the actor split is
# stable), minus the units listed in deferred.cmake.
include_guard(GLOBAL)

option(TWW_ALL_MODULES "Enable every game module regardless of its own option" OFF)

set(TWW_MODULES
        SSystem
        JSystem-core
        JSystem-J3D
        JSystem-2D-particle
        JSystem-studio
        framework
        m_Do
        d-core
        actors-1 actors-2 actors-3 actors-4 actors-5 actors-6)

# Modules whose every unit compiles (or is deferred); their options default to ON.
set(TWW_MODULES_READY
        SSystem
        JSystem-core
        JSystem-J3D
        JSystem-2D-particle
        JSystem-studio
        framework
        m_Do
        d-core
        actors-1
        actors-2
        actors-3
        actors-4
        actors-5
        actors-6)

set(TWW_ACTOR_CHUNKS 6)

# Collect <dir>/*.cpp|*.c (non-recursive unless RECURSE) relative to native/tww/src.
function(_tww_glob out)
    cmake_parse_arguments(G "" "" "DIRS;RECURSE;FILES" ${ARGN})
    set(_all)
    foreach (_d IN LISTS G_DIRS)
        file(GLOB _f CONFIGURE_DEPENDS "${TWW_ROOT}/src/${_d}/*.cpp" "${TWW_ROOT}/src/${_d}/*.c")
        list(APPEND _all ${_f})
    endforeach ()
    foreach (_d IN LISTS G_RECURSE)
        file(GLOB_RECURSE _f CONFIGURE_DEPENDS "${TWW_ROOT}/src/${_d}/*.cpp" "${TWW_ROOT}/src/${_d}/*.c")
        list(APPEND _all ${_f})
    endforeach ()
    foreach (_f IN LISTS G_FILES)
        list(APPEND _all "${TWW_ROOT}/src/${_f}")
    endforeach ()
    list(SORT _all)
    set(${out} ${_all} PARENT_SCOPE)
endfunction()

_tww_glob(TWW_SRC_SSystem RECURSE SSystem)
_tww_glob(TWW_SRC_JSystem-core
        DIRS JSystem/JKernel JSystem/JSupport JSystem/JUtility JSystem/JMath JSystem/JGadget
             JSystem/JFramework JSystem/JRenderer)
_tww_glob(TWW_SRC_JSystem-J3D
        DIRS JSystem/J3DGraphBase JSystem/J3DGraphAnimator JSystem/J3DGraphLoader JSystem/J3DU)
_tww_glob(TWW_SRC_JSystem-2D-particle DIRS JSystem/J2DGraph JSystem/JParticle)
_tww_glob(TWW_SRC_JSystem-studio DIRS JSystem/JStage JSystem/JMessage RECURSE JSystem/JStudio)
_tww_glob(TWW_SRC_framework DIRS f_pc f_op f_ap c FILES DynamicLink.cpp)
_tww_glob(TWW_SRC_m_Do DIRS m_Do)
_tww_glob(TWW_SRC_d-core DIRS d)

# Actors (src/d/actor, ~440 units) split into TWW_ACTOR_CHUNKS sorted chunks of equal size.
_tww_glob(_actors DIRS d/actor)
list(LENGTH _actors _n_actors)
math(EXPR _chunk "(${_n_actors} + ${TWW_ACTOR_CHUNKS} - 1) / ${TWW_ACTOR_CHUNKS}")
foreach (_i RANGE 1 ${TWW_ACTOR_CHUNKS})
    math(EXPR _begin "(${_i} - 1) * ${_chunk}")
    set(TWW_SRC_actors-${_i})
    if (_begin LESS _n_actors)
        list(SUBLIST _actors ${_begin} ${_chunk} TWW_SRC_actors-${_i})
    endif ()
endforeach ()

get_property(_deferred GLOBAL PROPERTY TWW_DEFERRED_UNITS)

set(_enabled)
foreach (_m IN LISTS TWW_MODULES)
    string(REPLACE "-" "_" _opt "TWW_MODULE_${_m}")
    if (_m IN_LIST TWW_MODULES_READY)
        option(${_opt} "Build the ${_m} game module" ON)
    else ()
        option(${_opt} "Build the ${_m} game module" OFF)
    endif ()

    set(_srcs ${TWW_SRC_${_m}})
    list(LENGTH _srcs _total)
    if (_deferred)
        list(REMOVE_ITEM _srcs ${_deferred})
    endif ()
    list(LENGTH _srcs _kept)
    math(EXPR _dropped "${_total} - ${_kept}")
    set_property(GLOBAL PROPERTY "TWW_MODULE_SOURCES:${_m}" "${_srcs}")

    if (${_opt} OR TWW_ALL_MODULES)
        add_library(${_m} OBJECT ${_srcs})
        target_link_libraries(${_m} PRIVATE tww_game_headers)
        set_target_properties(${_m} PROPERTIES FOLDER "game")
        list(APPEND _enabled ${_m})
        message(STATUS "tww_native: module ${_m}: ${_kept} units (${_dropped} deferred)")
    else ()
        message(STATUS "tww_native: module ${_m}: disabled (${_total} units, ${_dropped} deferred; -D${_opt}=ON)")
    endif ()
endforeach ()

# Aggregate target for every enabled module.
add_custom_target(tww_modules)
if (_enabled)
    add_dependencies(tww_modules ${_enabled})
endif ()

# `ninja tww_deferred` prints the deferred list with reasons.
set(_lines)
foreach (_u IN LISTS _deferred)
    file(RELATIVE_PATH _rel "${TWW_ROOT}" "${_u}")
    get_property(_why GLOBAL PROPERTY "TWW_DEFER_REASON:${_rel}")
    list(APPEND _lines "${_rel}: ${_why}")
endforeach ()
list(LENGTH _deferred _n_deferred)
file(WRITE "${CMAKE_BINARY_DIR}/tww_deferred.txt" "")
foreach (_l IN LISTS _lines)
    file(APPEND "${CMAKE_BINARY_DIR}/tww_deferred.txt" "${_l}\n")
endforeach ()
add_custom_target(tww_deferred
        COMMAND ${CMAKE_COMMAND} -E echo "${_n_deferred} deferred units:"
        COMMAND ${CMAKE_COMMAND} -E cat "${CMAKE_BINARY_DIR}/tww_deferred.txt"
        VERBATIM)
