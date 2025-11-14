# SPDX-License-Identifier: MIT
# Generate Vulkan SPIR-V and emit C headers.
# Uses glslangValidator with -V and no-space -I<dir>; optionally runs spirv-opt.

find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(PYTHON3_EXECUTABLE python3)
find_program(SPIRV_OPT spirv-opt) # optional

if(NOT GLSLANG_VALIDATOR)
  message(FATAL_ERROR "glslangValidator not found (install glslang-tools).")
endif()
if(NOT PYTHON3_EXECUTABLE)
  message(FATAL_ERROR "python3 not found.")
endif()

set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SPV_DIR "${CMAKE_BINARY_DIR}/generated_shaders/spv")
set(GENERATED_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${GENERATED_SPV_DIR}")
file(MAKE_DIRECTORY "${GENERATED_INCLUDE_DIR}")

file(GLOB SHADERS "${SHADER_SRC_DIR}/*.comp" "${SHADER_SRC_DIR}/*.glsl")

set(GEN_HEADERS "")
foreach(SRC IN LISTS SHADERS)
  get_filename_component(NAME ${SRC} NAME_WE)
  set(SPV_RAW "${GENERATED_SPV_DIR}/${NAME}.spv")
  set(SPV_OPT "${GENERATED_SPV_DIR}/${NAME}.opt.spv")
  set(HDR "${GENERATED_INCLUDE_DIR}/${NAME}_shader.h")

  add_custom_command(
    OUTPUT "${SPV_RAW}" "${SPV_OPT}" "${HDR}"
    COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${NAME} -> SPIR-V"
    COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.2 "-I${SHADER_SRC_DIR}" -o "${SPV_RAW}" "${SRC}"
    COMMAND ${CMAKE_COMMAND} -E env bash -c "[ -x \"${SPIRV_OPT}\" ] && \"${SPIRV_OPT}\" --strip-debug -O \"${SPV_RAW}\" -o \"${SPV_OPT}\" || true"
    COMMAND ${PYTHON3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py" "$<IF:$<AND:$<BOOL:${SPIRV_OPT}>,$<EXISTS:${SPIRV_OPT}>>,${SPV_OPT},${SPV_RAW}>" "${HDR}" "${NAME}_spv"
    DEPENDS "${SRC}" "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py"
    VERBATIM
  )

  list(APPEND GEN_HEADERS "${HDR}")
endforeach()

add_custom_target(gen_shaders ALL DEPENDS ${GEN_HEADERS})
include_directories("${GENERATED_INCLUDE_DIR}")
