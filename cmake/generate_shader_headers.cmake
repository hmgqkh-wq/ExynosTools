# SPDX-License-Identifier: MIT
# cmake/generate_shader_headers.cmake
# Generate SPIR-V (.spv) from assets/shaders/src/*.comp or *.glsl using glslangValidator
# then convert .spv to C headers with emit_spv_header.py.
#
# Include this file in the top-level CMakeLists.txt BEFORE the add_library/executable that builds exynostools:
#   include("${CMAKE_SOURCE_DIR}/cmake/generate_shader_headers.cmake")
#
# The generator creates headers in ${CMAKE_SOURCE_DIR}/include and spv files in ${CMAKE_BINARY_DIR}/generated_shaders/spv

find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(PYTHON3_EXECUTABLE python3)

if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found. Install glslang-tools or provide glslangValidator in PATH.")
endif()
if(NOT PYTHON3_EXECUTABLE)
    message(FATAL_ERROR "python3 not found. Install Python 3 to enable SPIR-V header generation.")
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
    get_filename_component(SRC_NAME ${SRC} NAME_WE) # e.g., bc1, bc_common
    set(SPV_OUT "${GENERATED_SPV_DIR}/${SRC_NAME}.spv")
    set(HEADER_OUT "${GENERATED_INCLUDE_DIR}/${SRC_NAME}_shader.h")

    add_custom_command(
        OUTPUT "${SPV_OUT}" "${HEADER_OUT}"
        COMMAND ${CMAKE_COMMAND} -E echo "Compiling shader: ${SRC_NAME}"
        COMMAND ${GLSLANG_VALIDATOR} -V "${SRC}" -o "${SPV_OUT}"
        COMMAND ${PYTHON3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py" "${SPV_OUT}" "${HEADER_OUT}" "${SRC_NAME}_spv"
        DEPENDS "${SRC}"
        COMMENT "Generating SPIR-V and header for ${SRC_NAME}"
        VERBATIM
    )

    list(APPEND GENERATED_HEADERS "${HEADER_OUT}")
endforeach()

# Custom target that builds all generated headers; ensure exynostools depends on it if the target exists
add_custom_target(gen_shaders ALL DEPENDS ${GENERATED_HEADERS})
if(TARGET exynostools)
    add_dependencies(exynostools gen_shaders)
endif()

# Make generated headers discoverable at configure/build time
include_directories("${GENERATED_INCLUDE_DIR}")
message(STATUS "Shader headers will be generated into ${GENERATED_INCLUDE_DIR}")
