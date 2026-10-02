# CMake toolchain: compile the game composite (cmake/composite) for the Switch
# with clang on the build machine itself, against devkitA64's newlib headers.
# On the largest generated chunk clang took 148 s and 1.5 GB where devkitA64's
# GCC took 255 s and 2.9 GB, and it runs natively instead of in the container.
# Objects only (COMPOSITE_STATIC_LINK=OFF): scripts/switch/build_host.sh links
# them into one relocatable object with devkitPro's tools. The composite uses
# no thread-local storage, so clang's objects need nothing GCC's -mtp=soft
# provides.
#
#   -DCMAKE_TOOLCHAIN_FILE=switch/composite/clang-switch.cmake
#   -DSWITCH_NEWLIB_INCLUDE=<devkitA64>/aarch64-none-elf/include

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_C_COMPILER_TARGET aarch64-none-elf)
# Nothing is linked here; compiler checks build a static library.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(SWITCH_NEWLIB_INCLUDE "" CACHE PATH "devkitA64's aarch64-none-elf/include")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES SWITCH_NEWLIB_INCLUDE)
if(NOT EXISTS "${SWITCH_NEWLIB_INCLUDE}/stdlib.h")
  message(FATAL_ERROR "Set SWITCH_NEWLIB_INCLUDE to devkitA64's aarch64-none-elf/include")
endif()
execute_process(COMMAND clang -print-resource-dir
                OUTPUT_VARIABLE CLANG_RESOURCE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)

# The Switch's CPU, newlib instead of the host's C library, and the position
# independence an NRO needs, as devkitA64's own flags give GCC.
set(CMAKE_C_FLAGS_INIT
    "-mcpu=cortex-a57 -nostdinc -isystem ${CLANG_RESOURCE_DIR}/include -isystem ${SWITCH_NEWLIB_INCLUDE} -D__SWITCH__ -ffunction-sections -fdata-sections -fPIC")
