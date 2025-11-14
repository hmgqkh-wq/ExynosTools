# cmake/generate_shader_headers.cmake
# SPDX-License-Identifier: MIT
# Safe include: creates gen_shaders target only once and exposes include dirs.
find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(PYTHON3_EXECUTABLE python3)

if(NOT GLSLANG_VALIDATOR)
  message(FATAL_ERROR "glslangValidator not found. Install glslang-tools.")
endif()
if(NOT PYTHON3_EXECUTABLE)
  message(FATAL_ERROR "python3 not found.")
endif()

if(TARGET gen_shaders)
  message(STATUS "gen_shaders target already exists; skipping")
  return()
endif()

set(SHADER_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/generate_spv_headers.sh")
if(NOT EXISTS "${SHADER_SCRIPT}")
  message(FATAL_ERROR "Required script not found: ${SHADER_SCRIPT}")
endif()

set(GENERATED_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${GENERATED_INCLUDE_DIR}")

add_custom_target(gen_shaders
  COMMAND ${CMAKE_COMMAND} -E echo "Running shader generator"
  COMMAND ${CMAKE_COMMAND} -E env bash "${SHADER_SCRIPT}"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generate SPIR-V headers (serial)"
  VERBATIM
)

# Add include dirs for generated headers and project sources
include_directories("${GENERATED_INCLUDE_DIR}" "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/include")
message(STATUS "Generated headers dir: ${GENERATED_INCLUDE_DIR}")
