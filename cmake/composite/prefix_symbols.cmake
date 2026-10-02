# Renames every global symbol defined in a relocatable object to PREFIX<name>,
# so a statically linked composite keeps its own copies of runtime functions
# (cpu.c and the interpreter) apart from the host's, as its shared library
# does. Undefined references (libc, libm) are left alone.
#
#   cmake -DNM=... -DOBJCOPY=... -DPREFIX=bwc_ -DINPUT=in.o -DOUTPUT=out.o -P prefix_symbols.cmake

foreach(VAR NM OBJCOPY PREFIX INPUT OUTPUT)
  if(NOT DEFINED ${VAR} OR "${${VAR}}" STREQUAL "")
    message(FATAL_ERROR "prefix_symbols.cmake: ${VAR} is required")
  endif()
endforeach()

execute_process(
  COMMAND "${NM}" -g --defined-only --format=posix "${INPUT}"
  OUTPUT_VARIABLE SYMBOL_TABLE
  RESULT_VARIABLE NM_RESULT)
if(NOT NM_RESULT EQUAL 0)
  message(FATAL_ERROR "nm failed on ${INPUT}")
endif()

string(REPLACE "\n" ";" SYMBOL_LINES "${SYMBOL_TABLE}")
set(RENAMES "")
set(COUNT 0)
foreach(LINE IN LISTS SYMBOL_LINES)
  if(LINE MATCHES "^([^ ]+) ")
    string(APPEND RENAMES "${CMAKE_MATCH_1} ${PREFIX}${CMAKE_MATCH_1}\n")
    math(EXPR COUNT "${COUNT} + 1")
  endif()
endforeach()
if(COUNT EQUAL 0)
  message(FATAL_ERROR "${INPUT} defines no global symbols")
endif()

set(RENAME_FILE "${OUTPUT}.renames")
file(WRITE "${RENAME_FILE}" "${RENAMES}")
execute_process(
  COMMAND "${OBJCOPY}" --redefine-syms "${RENAME_FILE}" "${INPUT}" "${OUTPUT}"
  RESULT_VARIABLE OBJCOPY_RESULT)
if(NOT OBJCOPY_RESULT EQUAL 0)
  message(FATAL_ERROR "objcopy failed renaming ${INPUT}")
endif()
message(STATUS "Prefixed ${COUNT} composite symbols with ${PREFIX}")
