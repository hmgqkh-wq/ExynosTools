# cmake/generate_shader_headers.cmake
# SPDX-License-Identifier: MIT
# Centralized shader generation target that invokes scripts/generate_spv_headers.sh serially.
# Safe to include multiple times; exports include dirs so project sources can find headers like logging.h.

find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(PYTHON3_EXECUTABLE python3)

if(NOT GLSLANG_VALIDATOR)
  message(FATAL_ERROR "glslangValidator not found. Install glslang-tools or put glslangValidator in PATH.")
endif()
if(NOT PYTHON3_EXECUTABLE)
  message(FATAL_ERROR "python3 not found. Install Python 3.")
endif()

if(TARGET gen_shaders)
  message(STATUS "gen_shaders target already defined; skipping duplicate creation")
  return()
endif()

set(SHADER_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/generate_spv_headers.sh")
if(NOT EXISTS "${SHADER_SCRIPT}")
  message(FATAL_ERROR "Required script not found: ${SHADER_SCRIPT}")
endif()

set(GENERATED_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${GENERATED_INCLUDE_DIR}")

add_custom_target(gen_shaders
  COMMAND ${CMAKE_COMMAND} -E echo "Running centralized shader generator script"
  COMMAND ${CMAKE_COMMAND} -E env bash "${SHADER_SCRIPT}"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generate SPIR-V and C headers for shaders (serial)"
  VERBATIM
)

# Make sure main targets wait for generated headers; adapt these names if your targets differ
foreach(tgt IN ITEMS exynostools xeno_wrapper)
  if(TARGET ${tgt})
    add_dependencies(${tgt} gen_shaders)
  endif()
endforeach()

# Make generated headers discoverable to build and ensure project source headers are also found.
# Some sources include project headers like "logging.h" from src/ — add that too.
include_directories(
  "${GENERATED_INCLUDE_DIR}"
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/include"
)

message(STATUS "Using centralized shader generator: ${SHADER_SCRIPT}")
message(STATUS "Generated headers dir: ${GENERATED_INCLUDE_DIR}")
