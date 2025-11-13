# generate_shader_header.cmake
# Robust SPIR-V binary -> uint32_t[] C header generator.
# If SPIRV_FILE equals "__MISSING__" or file not present, a safe empty header is emitted.
# Expects SPIRV_FILE, HEADER_FILE, VAR_NAME on the command line via -D

if(NOT DEFINED SPIRV_FILE)
  message(FATAL_ERROR "SPIRV_FILE not set")
endif()
if(NOT DEFINED HEADER_FILE)
  message(FATAL_ERROR "HEADER_FILE not set")
endif()
if(NOT DEFINED VAR_NAME)
  message(FATAL_ERROR "VAR_NAME not set")
endif()

file(TO_CMAKE_PATH "${SPIRV_FILE}" __spv_in)
file(TO_CMAKE_PATH "${HEADER_FILE}" __hdr_out)

# If SPIRV_FILE is explicitly the sentinel value, emit an empty safe header
if("${SPIRV_FILE}" STREQUAL "__MISSING__" OR NOT EXISTS "${__spv_in}")
  message(STATUS "generate_shader_header: SPIR-V missing, producing fallback header ${__hdr_out}")
  file(WRITE "${__hdr_out}" "#pragma once\n#include <stdint.h>\nstatic const uint32_t ${VAR_NAME}[] = { 0x00000000 }; \nstatic const unsigned int ${VAR_NAME}_len = 4; \n")
  return()
endif()

# Read binary into variable _BIN_DATA (raw bytes)
file(READ "${__spv_in}" _BIN_DATA)

string(LENGTH "${_BIN_DATA}" _len_bytes)
math(EXPR _num_words "(${_len_bytes} + 3) / 4")

set(_words "")
foreach(i RANGE 0 ${_num_words} 1)
  math(EXPR off "${i} * 4")
  set(_w 0)
  foreach(b RANGE 0 3)
    math(EXPR idx "${off} + ${b}")
    if(idx GREATER_EQUAL _len_bytes)
      break()
    endif()
    string(SUBSTRING "${_BIN_DATA}" ${idx} 1 _byte)
    # Convert single character to its numeric byte value using /bin/printf (portable on CI)
    execute_process(COMMAND /bin/printf "%d" "'${_byte}"
      OUTPUT_VARIABLE _bval
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    math(EXPR _w "${_w} + (${_bval} << (${b} * 8))")
  endforeach()
  string(APPEND _words "0x${_w}, ")
endforeach()

# Write header: uint32_t array and length in bytes
file(WRITE "${__hdr_out}" "#pragma once\n#include <stdint.h>\nstatic const uint32_t ${VAR_NAME}[] = { ${_words} };\nstatic const unsigned int ${VAR_NAME}_len = sizeof(${VAR_NAME});\n")
message(STATUS "Generated header ${__hdr_out} (${_num_words} words, ${_len_bytes} bytes).")
