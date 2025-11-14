# SPDX-License-Identifier: MIT
# cmake/generate_shader_headers.cmake
# Robust SPIR-V generation pipeline: compile (glslangValidator) -> optional optimize (spirv-opt)
# -> emit header (emit_spv_header.py). Avoids parallel race conditions by making each shader
# a single custom-command producing all byproducts and listing them in BYPRODUCTS.

find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(PYTHON3_EXECUTABLE python3)
find_program(SPIRV_OPT_BIN spirv-opt)

if(NOT GLSLANG_VALIDATOR)
  message(FATAL_ERROR "glslangValidator not found. Install glslang-tools or provide glslangValidator in PATH.")
endif()
if(NOT PYTHON3_EXECUTABLE)
  message(FATAL_ERROR "python3 not found. Install Python 3.")
endif()

if(SPIRV_OPT_BIN)
  message(STATUS "spirv-opt found at: ${SPIRV_OPT_BIN}")
  set(HAVE_SPIRV_OPT TRUE)
else()
  message(STATUS "spirv-opt not found; will skip SPIR-V optimization")
  set(HAVE_SPIRV_OPT FALSE)
endif()

set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SPV_DIR "${CMAKE_BINARY_DIR}/generated_shaders/spv")
set(GENERATED_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

file(MAKE_DIRECTORY "${GENERATED_SPV_DIR}")
file(MAKE_DIRECTORY "${GENERATED_INCLUDE_DIR}")

file(GLOB SHADER_SRC_FILES RELATIVE "${SHADER_SRC_DIR}" "${SHADER_SRC_DIR}/*.comp" "${SHADER_SRC_DIR}/*.glsl")
if(NOT SHADER_SRC_FILES)
  message(WARNING "No shader source files found in ${SHADER_SRC_DIR}")
endif()

set(GENERATED_HEADERS "")
foreach(REL_SRC IN LISTS SHADER_SRC_FILES)
  set(SRC "${SHADER_SRC_DIR}/${REL_SRC}")
  get_filename_component(SRC_NAME ${REL_SRC} NAME_WE)

  set(SPV_RAW "${GENERATED_SPV_DIR}/${SRC_NAME}.spv")
  set(SPV_OPT "${GENERATED_SPV_DIR}/${SRC_NAME}.opt.spv")
  set(HEADER_OUT "${GENERATED_INCLUDE_DIR}/${SRC_NAME}_shader.h")

  if(HAVE_SPIRV_OPT)
    set(BYPRODUCTS "${SPV_RAW};${SPV_OPT};${HEADER_OUT}")
    set(CMD_SH
      "${CMAKE_COMMAND} -E echo '>> gen_shaders: ${SRC_NAME} (compile -> optimize -> emit)'"
      "&& \"${GLSLANG_VALIDATOR}\" -V --target-env vulkan1.2 \"-I${SHADER_SRC_DIR}\" -o \"${SPV_RAW}\" \"${SRC}\""
      "&& \"${SPIRV_OPT_BIN}\" --strip-debug -O \"${SPV_RAW}\" -o \"${SPV_OPT}\""
      "&& \"${PYTHON3_EXECUTABLE}\" \"${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py\" \"${SPV_OPT}\" \"${HEADER_OUT}\" \"${SRC_NAME}_spv\""
    )
  else()
    set(BYPRODUCTS "${SPV_RAW};${HEADER_OUT}")
    set(CMD_SH
      "${CMAKE_COMMAND} -E echo '>> gen_shaders: ${SRC_NAME} (compile -> emit)'"
      "&& \"${GLSLANG_VALIDATOR}\" -V --target-env vulkan1.2 \"-I${SHADER_SRC_DIR}\" -o \"${SPV_RAW}\" \"${SRC}\""
      "&& \"${PYTHON3_EXECUTABLE}\" \"${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py\" \"${SPV_RAW}\" \"${HEADER_OUT}\" \"${SRC_NAME}_spv\""
    )
  endif()

  add_custom_command(
    OUTPUT "${HEADER_OUT}"
    BYPRODUCTS ${BYPRODUCTS}
    COMMAND ${CMAKE_COMMAND} -E echo "Starting shader pipeline for ${SRC_NAME}"
    COMMAND ${CMAKE_COMMAND} -E env bash -c "${CMD_SH}"
    DEPENDS "${SRC}" "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py"
    COMMENT "Generate SPV (and header) for ${SRC_NAME}"
    VERBATIM
  )

  list(APPEND GENERATED_HEADERS "${HEADER_OUT}")
endforeach()

add_custom_target(gen_shaders ALL DEPENDS ${GENERATED_HEADERS})

# Ensure main targets wait for generated headers
foreach(tgt IN ITEMS exynostools xeno_wrapper)
  if(TARGET ${tgt})
    add_dependencies(${tgt} gen_shaders)
  endif()
endforeach()

include_directories("${GENERATED_INCLUDE_DIR}")
message(STATUS "Shader source: ${SHADER_SRC_DIR}")
message(STATUS "SPV output: ${GENERATED_SPV_DIR}")
message(STATUS "Headers output: ${GENERATED_INCLUDE_DIR}")
