# SPDX-License-Identifier: MIT
# cmake/generate_shader_headers.cmake
# Robust shader generation: run cpp preprocessor to expand #include "..." from
# assets/shaders/src, then pipe to glslangValidator to produce .spv and
# emit_spv_header.py to produce C headers.
#
# Usage: include("${CMAKE_SOURCE_DIR}/cmake/generate_shader_headers.cmake")
# Place this include early in the top-level CMakeLists.txt, before creating the
# exynostools target so add_dependencies(exynostools gen_shaders) works.

find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(PYTHON3_EXECUTABLE python3)
find_program(CPP_EXECUTABLE cpp)

if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found. Install glslang-tools or provide glslangValidator in PATH.")
endif()
if(NOT PYTHON3_EXECUTABLE)
    message(FATAL_ERROR "python3 not found. Install Python 3 to enable SPIR-V header generation.")
endif()
if(NOT CPP_EXECUTABLE)
    message(FATAL_ERROR "C preprocessor (cpp) not found. Install cpp or libc6-dev (Ubuntu) to enable shader include expansion.")
endif()

set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SPV_DIR "${CMAKE_BINARY_DIR}/generated_shaders/spv")
set(GENERATED_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

file(GLOB SHADER_SRC_FILES RELATIVE "${SHADER_SRC_DIR}" "${SHADER_SRC_DIR}/*.comp" "${SHADER_SRC_DIR}/*.glsl")

if(NOT SHADER_SRC_FILES)
    message(WARNING "No shader source files found in ${SHADER_SRC_DIR}")
endif()

file(MAKE_DIRECTORY "${GENERATED_SPV_DIR}")
file(MAKE_DIRECTORY "${GENERATED_INCLUDE_DIR}")

set(GENERATED_HEADERS "")
foreach(REL_SRC IN LISTS SHADER_SRC_FILES)
    set(SRC "${SHADER_SRC_DIR}/${REL_SRC}")
    file(RELATIVE_PATH SRC_REL "${SHADER_SRC_DIR}" "${SRC}")
    get_filename_component(SRC_NAME ${SRC} NAME_WE) # e.g., bc1, bc_common
    set(SPV_OUT "${GENERATED_SPV_DIR}/${SRC_NAME}.spv")
    set(HEADER_OUT "${GENERATED_INCLUDE_DIR}/${SRC_NAME}_shader.h")

    # We run cpp -P -I <shader_src_dir> <file> to expand #include "foo"
    # then pipe to glslangValidator reading from stdin ("-") producing .spv.
    # Use a single shell command so the pipe is executed correctly by CMake.
    add_custom_command(
        OUTPUT "${SPV_OUT}" "${HEADER_OUT}"
        COMMAND ${CMAKE_COMMAND} -E echo "Preprocessing and compiling shader: ${SRC_NAME}"
        COMMAND ${CPP_EXECUTABLE} -P -I "${SHADER_SRC_DIR}" "${SRC}" | ${GLSLANG_VALIDATOR} -V -o "${SPV_OUT}" -
        COMMAND ${PYTHON3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py" "${SPV_OUT}" "${HEADER_OUT}" "${SRC_NAME}_spv"
        DEPENDS "${SRC}" "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py"
        COMMENT "Generating SPIR-V and header for ${SRC_NAME} (cpp -> glslangValidator -> emit_spv_header.py)"
        VERBATIM
    )

    list(APPEND GENERATED_HEADERS "${HEADER_OUT}")
endforeach()

# Create a target that will generate all headers; make it ALL so CI generation occurs by default
add_custom_target(gen_shaders ALL DEPENDS ${GENERATED_HEADERS})

# If your main target exists, ensure it depends on gen_shaders so headers are ready before compile
if(TARGET exynostools)
    add_dependencies(exynostools gen_shaders)
endif()

# Make generated headers discoverable at configure/build time
include_directories("${GENERATED_INCLUDE_DIR}")
message(STATUS "Shader headers will be generated into ${GENERATED_INCLUDE_DIR}")
message(STATUS "SPIR-V files will be generated into ${GENERATED_SPV_DIR}")
