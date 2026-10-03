# tww_sdk_gf: TWW's own GF sources (native/tww/src/dolphin/gf) compiled into tww_sdk
# (docs/NATIVE_PORT_PHASE2_3.md, step 2.6d).
#
# GF is the SDK's "fast" GX layer: each function writes raw BP/XF/CP register commands through
# GXCmd1u8/u16/u32. Aurora ships the GF headers but no GF bodies, so the bodies come from the decomp,
# as Dusklight compiles TP's libs/dolphin/src/gf/GF*.cpp (files.cmake:1406). Aurora's GXCmd1u*
# write into the same FIFO as every other GX call (lib/gx/fifo.hpp), outside a display list as well
# as inside one, and its command processor parses LOAD_BP/CP/XF_REG in both cases.
#
# - Compiled against Aurora's SDK headers plus the forwarders in native/include/sdk (the TWW-only
#   names dolphin/gf/GF.h, dolphin/gf/GFTransform.h, dolphin/os/OS.h), with the TWW-only GF
#   declarations from native/include/sdk/tww_gf_extras.h. The same include order as the game in
#   TWW_SDK_HEADERS=aurora mode, whatever TWW_SDK_HEADERS is: native/tww/include is never on the
#   path, and neither are the game's flags (tww_game_headers).
# - An OBJECT library of its own (this file, not sdk.cmake, so 2.6d does not touch the glob), whose
#   objects go into tww_sdk.
# - One TARGET_PC edit (GFGeometry.cpp): Aurora ignores CP_REG_ARRAYBASE, so GFSetArraySized writes
#   Aurora's 64-bit array-base command and GFSetArray, which has no size, stops with OSPanic.
#   The other four units are unchanged decomp code.
#
# Needs Aurora, so it is only built with TWW_WITH_AURORA=ON (after sdk.cmake has made tww_sdk).

if (NOT TWW_WITH_AURORA OR NOT TARGET tww_sdk)
    return()
endif ()

set(TWW_SDK_GF_SOURCES
        "${TWW_ROOT}/src/dolphin/gf/GFGeometry.cpp"
        "${TWW_ROOT}/src/dolphin/gf/GFLight.cpp"
        "${TWW_ROOT}/src/dolphin/gf/GFPixel.cpp"
        "${TWW_ROOT}/src/dolphin/gf/GFTev.cpp"
        "${TWW_ROOT}/src/dolphin/gf/GFTransform.cpp")

add_library(tww_sdk_gf OBJECT ${TWW_SDK_GF_SOURCES})
# Forwarders first, then Aurora's include (from the aurora::* targets below), as in GameConfig's
# aurora mode. Only GF, GD, GX, MTX and OS are needed; the full list keeps the defines identical
# to tww_sdk's.
target_include_directories(tww_sdk_gf PRIVATE "${TWW_NATIVE_ROOT}/include/sdk")
target_compile_definitions(tww_sdk_gf PRIVATE MTX_USE_PS=1)
# The decomp's vertex-descriptor switches leave most enumerators to the hardware defaults on purpose.
target_compile_options(tww_sdk_gf PRIVATE -Wno-switch)
target_link_libraries(tww_sdk_gf PRIVATE ${TWW_AURORA_LIBS})

target_sources(tww_sdk PRIVATE $<TARGET_OBJECTS:tww_sdk_gf>)

# The "gf" smoke test (native/sdk/tests/sdk_gf.cpp) includes dolphin/gf/GF.h through its forwarder,
# as the game will.
target_include_directories(tww_sdk_smoke PRIVATE "${TWW_NATIVE_ROOT}/include/sdk")

# The ThreadSanitizer smoke program compiles tww_sdk's sources itself (sdk.cmake); it gets the GF
# objects uninstrumented, as it gets Aurora's libraries.
if (TARGET tww_sdk_smoke_tsan)
    target_sources(tww_sdk_smoke_tsan PRIVATE $<TARGET_OBJECTS:tww_sdk_gf>)
    target_include_directories(tww_sdk_smoke_tsan PRIVATE "${TWW_NATIVE_ROOT}/include/sdk")
endif ()
