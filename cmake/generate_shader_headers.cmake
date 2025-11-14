# SPDX-License-Identifier: MIT
# cmake/generate_shader_headers.cmake
# Generate SPIR-V header files from assets/shaders/src/*.comp using glslangValidator.
# Place this file in cmake/, then add: include(cmake/generate_shader_headers.cmake)
# to your top-level CMakeLists.txt before add_library/executable.

find_program(GLSLANG_VALIDATOR_EXECUTABLE glslangValidator)

if(NOT GLSLANG_VALIDATOR_EXECUTABLE)
    message(WARNING "glslangValidator not found; shader header generation will be skipped")
    return()
endif()

file(GLOB SHADER_COMP_FILES "${CMAKE_SOURCE_DIR}/assets/shaders/src/*.comp")

foreach(SRC_PATH IN LISTS SHADER_COMP_FILES)
    get_filename_component(SRC_NAME ${SRC_PATH} NAME_WE)       # e.g., bc1
    set(SPV_OUT "${CMAKE_BINARY_DIR}/shaders/${SRC_NAME}.spv")
    set(HEADER_OUT "${CMAKE_SOURCE_DIR}/include/${SRC_NAME}_shader.h")
    add_custom_command(
        OUTPUT ${SPV_OUT} ${HEADER_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
        COMMAND ${GLSLANG_VALIDATOR_EXECUTABLE} -V "${SRC_PATH}" -o "${SPV_OUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/include"
        COMMAND ${CMAKE_COMMAND} -DINPUT_SPV="${SPV_OUT}" -DOUTPUT_H="${HEADER_OUT}" -P "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.cmake"
        DEPENDS ${SRC_PATH}
        COMMENT "Compiling ${SRC_NAME}.comp -> ${HEADER_OUT}"
        VERBATIM
    )
    list(APPEND GENERATED_SHADER_HEADERS ${HEADER_OUT})
endforeach()

# Ensure headers are built before the main target
if(TARGET exynostools)
    add_custom_target(gen_shaders ALL DEPENDS ${GENERATED_SHADER_HEADERS})
    add_dependencies(exynostools gen_shaders)
endif()
