# SPDX-License-Identifier: MIT
# Centralized shader generation target that invokes scripts/generate_spv_headers.sh serially.

find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(PYTHON3_EXECUTABLE python3)

if(NOT GLSLANG_VALIDATOR)
  message(FATAL_ERROR "glslangValidator not found. Install glslang-tools or put glslangValidator in PATH.")
endif()
if(NOT PYTHON3_EXECUTABLE)
  message(FATAL_ERROR "python3 not found. Install Python 3.")
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

foreach(tgt IN ITEMS exynostools xeno_wrapper)
  if(TARGET ${tgt})
    add_dependencies(${tgt} gen_shaders)
  endif()
endforeach()

include_directories("${GENERATED_INCLUDE_DIR}")
message(STATUS "Using centralized shader generator: ${SHADER_SCRIPT}")
message(STATUS "Generated headers dir: ${GENERATED_INCLUDE_DIR}")
add_custom_target(gen_shaders
  COMMAND ${CMAKE_COMMAND} -E echo "Running centralized shader generator script"
  COMMAND ${CMAKE_COMMAND} -E env bash "${CMAKE_SOURCE_DIR}/scripts/generate_spv_headers.sh"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generate SPIR-V and C headers for shaders (serial)"
  VERBATIM
)
