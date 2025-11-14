# generate_shader_header.cmake
# Emits two symbols:
#   static const uint32_t <VAR_NAME>[] = { ... };
#   static const unsigned int <VAR_NAME>_len = <byte_count>;

if(NOT DEFINED SPIRV_FILE OR NOT DEFINED HEADER_FILE OR NOT DEFINED VAR_NAME)
  message(FATAL_ERROR "SPIRV_FILE, HEADER_FILE, VAR_NAME must be set")
endif()

file(TO_CMAKE_PATH "${SPIRV_FILE}" __spv_in)
file(TO_CMAKE_PATH "${HEADER_FILE}" __hdr_out)

if("${SPIRV_FILE}" STREQUAL "__MISSING__" OR NOT EXISTS "${__spv_in}")
  message(STATUS "generate_shader_header: SPIR-V missing, producing fallback ${__hdr_out}")
  file(WRITE "${__hdr_out}" "#pragma once
#include <stdint.h>
static const uint32_t ${VAR_NAME}[] = { 0x00000000 };
static const unsigned int ${VAR_NAME}_len = 4;
")
  return()
endif()

file(READ "${__spv_in}" _BIN_DATA)
string(LENGTH "${_BIN_DATA}" _len_bytes)
math(EXPR _num_words "(${_len_bytes} + 3) / 4")

set(_words "")
foreach(i RANGE 0 ${_num_words} 1)
  math(EXPR off "${i} * 4")
  set(_w 0)
  foreach(b RANGE 0 3)
    math(EXPR idx "${off} + ${b}")
    if(idx LESS _len_bytes)
      string(SUBSTRING "${_BIN_DATA}" ${idx} 1 _byte)
      execute_process(COMMAND /bin/printf "%d" "'${_byte}"
        OUTPUT_VARIABLE _bval
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      math(EXPR _w "${_w} + (${_bval} << (${b} * 8))")
    endif()
  endforeach()
  execute_process(COMMAND /bin/printf "%08x" "${_w}"
    OUTPUT_VARIABLE _hex
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  string(APPEND _words "0x${_hex}, ")
endforeach()

file(WRITE "${__hdr_out}" "#pragma once
#include <stdint.h>
static const uint32_t ${VAR_NAME}[] = { ${_words} };
static const unsigned int ${VAR_NAME}_len = ${_len_bytes};
")
message(STATUS "Generated ${__hdr_out} (${_num_words} words, ${_len_bytes} bytes)")
