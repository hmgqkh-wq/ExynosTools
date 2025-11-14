# SPDX-License-Identifier: MIT
# cmake/generate_shader_headers.cmake
# Generate SPIR-V from assets/shaders/src with glslangValidator (-V, Vulkan),
# then emit C headers via emit_spv_header.py into include/.

find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(PYTHON3_EXECUTABLE python3)

if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found. Install glslang-tools (Ubuntu) or provide glslangValidator in PATH.")
endif()
if(NOT PYTHON3_EXECUTABLE)
    message(FATAL_ERROR "python3 not found. Install Python 3.")
endif()

set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SPV_DIR "${CMAKE_BINARY_DIR}/generated_shaders/spv")
set(GENERATED_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

file(GLOB SHADER_SRC_FILES "${SHADER_SRC_DIR}/*.comp" "${SHADER_SRC_DIR}/*.glsl")
if(NOT SHADER_SRC_FILES)
    message(WARNING "No shader source files found in ${SHADER_SRC_DIR}")
endif()

file(MAKE_DIRECTORY "${GENERATED_SPV_DIR}")
file(MAKE_DIRECTORY "${GENERATED_INCLUDE_DIR}")

set(GENERATED_HEADERS "")
foreach(SRC IN LISTS SHADER_SRC_FILES)
    get_filename_component(SRC_NAME ${SRC} NAME_WE)
    set(SPV_OUT "${GENERATED_SPV_DIR}/${SRC_NAME}.spv")
    set(HEADER_OUT "${GENERATED_INCLUDE_DIR}/${SRC_NAME}_shader.h")

    # Compile with Vulkan (-V). Provide include path (-I) so #include "bc_common.glsl" works.
    add_custom_command(
        OUTPUT "${SPV_OUT}" "${HEADER_OUT}"
        COMMAND ${CMAKE_COMMAND} -E echo "Compiling (Vulkan) and emitting header: ${SRC_NAME}"
        COMMAND ${GLSLANG_VALIDATOR} -V -I "${SHADER_SRC_DIR}" -o "${SPV_OUT}" "${SRC}"
        COMMAND ${PYTHON3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py" "${SPV_OUT}" "${HEADER_OUT}" "${SRC_NAME}_spv"
        DEPENDS "${SRC}" "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py"
        COMMENT "glslangValidator -V -I ${SHADER_SRC_DIR} ${SRC} -> ${SPV_OUT}; emit header -> ${HEADER_OUT}"
        VERBATIM
    )

    list(APPEND GENERATED_HEADERS "${HEADER_OUT}")
endforeach()

add_custom_target(gen_shaders ALL DEPENDS ${GENERATED_HEADERS})

# If your main target exists, ensure it waits for generated headers
if(TARGET exynostools)
    add_dependencies(exynostools gen_shaders)
endif()
if(TARGET xeno_wrapper)
    add_dependencies(xeno_wrapper gen_shaders)
endif()

# Make generated headers discoverable
include_directories("${GENERATED_INCLUDE_DIR}")
message(STATUS "SPV -> ${GENERATED_SPV_DIR}")
message(STATUS "Headers -> ${GENERATED_INCLUDE_DIR}")
