# tww_dsp_hle: the DSP behind tww_sdk's DSP library (step 5.A of docs/NATIVE_PORT_PHASE4_6.md,
# decisions H6 and H10). Dolphin's high-level DSP emulation (DSPHLE with its Zelda ucode, GPLv2+;
# this repository is GPLv3), compiled from the RecompCore checkout (ref/recompcore) the way the
# iOS and Switch hosts compile it (apple/ios/CMakeLists.txt, switch/host/CMakeLists.txt), behind
# a small API of its own (native/dsp_hle/tww_dsp_hle.h) so tww_sdk never sees a Dolphin header.
#
# TWW_RECOMPCORE_DIR is the RecompCore source checkout. By default it is ref/recompcore of the
# repository, or of the main checkout when this is a git worktree (build/lanes/<lane>).
# Only its sources are read; nothing is generated inside it.

if (NOT TWW_WITH_AURORA)
    return() # only tww_sdk uses it, and fmt comes with Aurora
endif ()

set(TWW_DSP_HLE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../dsp_hle")
cmake_path(NORMAL_PATH TWW_DSP_HLE_ROOT)

if (NOT TWW_RECOMPCORE_DIR)
    set(_tww_recompcore "${TWW_NATIVE_ROOT}/../ref/recompcore")
    if (NOT EXISTS "${_tww_recompcore}/Source/Core/Core/HW/DSPHLE/DSPHLE.cpp")
        # A git worktree has no ref/: use the main checkout's (git's common directory's parent).
        execute_process(COMMAND git -C "${TWW_NATIVE_ROOT}" rev-parse --path-format=absolute --git-common-dir
                OUTPUT_VARIABLE _tww_git_common OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if (_tww_git_common)
            set(_tww_recompcore "${_tww_git_common}/../ref/recompcore")
        endif ()
    endif ()
    cmake_path(NORMAL_PATH _tww_recompcore)
    set(TWW_RECOMPCORE_DIR "${_tww_recompcore}" CACHE PATH
            "RecompCore source checkout (Dolphin's DSPHLE for the DSP, decision H6)")
endif ()
if (NOT EXISTS "${TWW_RECOMPCORE_DIR}/Source/Core/Core/HW/DSPHLE/UCodes/Zelda.cpp")
    message(FATAL_ERROR "tww_native: TWW_RECOMPCORE_DIR (${TWW_RECOMPCORE_DIR}) is not a RecompCore "
            "checkout (needs Source/Core/Core/HW/DSPHLE)")
endif ()

set(_tww_dsphle "${TWW_RECOMPCORE_DIR}/Source/Core/Core/HW/DSPHLE")
add_library(tww_dsp_hle STATIC
        # The ARAM accelerator the ucodes read samples through.
        "${TWW_RECOMPCORE_DIR}/Source/Core/Core/DSP/DSPAccelerator.cpp"
        ${_tww_dsphle}/DSPHLE.cpp
        ${_tww_dsphle}/MailHandler.cpp
        ${_tww_dsphle}/UCodes/AESnd.cpp
        ${_tww_dsphle}/UCodes/ASnd.cpp
        ${_tww_dsphle}/UCodes/AX.cpp
        ${_tww_dsphle}/UCodes/AXWii.cpp
        ${_tww_dsphle}/UCodes/CARD.cpp
        ${_tww_dsphle}/UCodes/GBA.cpp
        ${_tww_dsphle}/UCodes/INIT.cpp
        ${_tww_dsphle}/UCodes/ROM.cpp
        ${_tww_dsphle}/UCodes/UCodes.cpp
        ${_tww_dsphle}/UCodes/Zelda.cpp
        ${_tww_dsphle}/UCodes/ZeldaUCodesTable.cpp
        "${TWW_DSP_HLE_ROOT}/dsp_hle_backend.cpp"
        "${TWW_DSP_HLE_ROOT}/dsp_common_shim.cpp")
target_include_directories(tww_dsp_hle PUBLIC "${TWW_DSP_HLE_ROOT}")
target_include_directories(tww_dsp_hle PRIVATE
        "${TWW_RECOMPCORE_DIR}/Source/Core"
        "${TWW_RECOMPCORE_DIR}/Source"
        "${TWW_RECOMPCORE_DIR}/Externals"
        "${TWW_DSP_HLE_ROOT}/generated")
target_compile_definitions(tww_dsp_hle PRIVATE
        _ARCH_64=1 _M_ARM_64=1 _DEFAULT_SOURCE __STDC_CONSTANT_MACROS __STDC_LIMIT_MACROS)
target_compile_features(tww_dsp_hle PRIVATE cxx_std_23)
# Dolphin's code, not ours: its warnings are not acted on here.
target_compile_options(tww_dsp_hle PRIVATE -fno-strict-aliasing -w)
# Aurora's fmt (one fmt per binary) and the host zlib (Common::HashAdler32).
target_link_libraries(tww_dsp_hle PRIVATE fmt::fmt z)
