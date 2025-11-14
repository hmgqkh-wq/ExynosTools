# SPDX-License-Identifier: MIT
# cmake/generate_shader_headers.cmake
# Robust SPIR-V generation: find spirv-opt at configure time and call it by path.
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
    message(STATUS "Found spirv-opt at: ${SPIRV_OPT_BIN}")
    set(HAVE_SPIRV_OPT TRUE)
else()
    message(STATUS "spirv-opt not found; skipping SPIR-V optimization")
    set(HAVE_SPIRV_OPT FALSE)
endif()

set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SPV_DIR "${CMAKE_BINARY_DIR}/generated_shaders/spv")
set(GENERATED_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
file(MAKE_DIRECTORY "${GENERATED_SPV_DIR}")
file(MAKE_DIRECTORY "${GENERATED_INCLUDE_DIR}")

file(GLOB SHADER_SRC_FILES "${SHADER_SRC_DIR}/*.comp" "${SHADER_SRC_DIR}/*.glsl")
if(NOT SHADER_SRC_FILES)
    message(WARNING "No shader source files found in ${SHADER_SRC_DIR}")
endif()

set(GENERATED_HEADERS "")
foreach(SRC IN LISTS SHADER_SRC_FILES)
    get_filename_component(SRC_NAME ${SRC} NAME_WE)
    set(SPV_RAW "${GENERATED_SPV_DIR}/${SRC_NAME}.spv")
    set(SPV_OPT "${GENERATED_SPV_DIR}/${SRC_NAME}.opt.spv")
    set(HEADER_OUT "${GENERATED_INCLUDE_DIR}/${SRC_NAME}_shader.h")

    if(HAVE_SPIRV_OPT)
        add_custom_command(
            OUTPUT "${SPV_RAW}" "${SPV_OPT}" "${HEADER_OUT}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${SRC_NAME} -> ${SPV_RAW}"
            COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.2 "-I${SHADER_SRC_DIR}" -o "${SPV_RAW}" "${SRC}"
            COMMAND ${CMAKE_COMMAND} -E echo "Optimizing ${SPV_RAW} -> ${SPV_OPT} using ${SPIRV_OPT_BIN}"
            COMMAND "${SPIRV_OPT_BIN}" --strip-debug -O "${SPV_RAW}" -o "${SPV_OPT}"
            COMMAND ${CMAKE_COMMAND} -E echo "Emitting header ${HEADER_OUT} from ${SPV_OPT}"
            COMMAND ${PYTHON3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py" "${SPV_OPT}" "${HEADER_OUT}" "${SRC_NAME}_spv"
            DEPENDS "${SRC}" "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py"
            COMMENT "Generate optimized SPV and header for ${SRC_NAME}"
            VERBATIM
        )
    else()
        add_custom_command(
            OUTPUT "${SPV_RAW}" "${HEADER_OUT}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${SRC_NAME} -> ${SPV_RAW}"
            COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.2 "-I${SHADER_SRC_DIR}" -o "${SPV_RAW}" "${SRC}"
            COMMAND ${CMAKE_COMMAND} -E echo "Emitting header ${HEADER_OUT} from ${SPV_RAW}"
            COMMAND ${PYTHON3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py" "${SPV_RAW}" "${HEADER_OUT}" "${SRC_NAME}_spv"
            DEPENDS "${SRC}" "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py"
            COMMENT "Generate SPV and header for ${SRC_NAME}"
            VERBATIM
        )
    endif()

    list(APPEND GENERATED_HEADERS "${HEADER_OUT}")
endforeach()

add_custom_target(gen_shaders ALL DEPENDS ${GENERATED_HEADERS})

# Make generated include dir visible
include_directories("${GENERATED_INCLUDE_DIR}")
message(STATUS "Shader source dir: ${SHADER_SRC_DIR}")
message(STATUS "Generated SPV dir: ${GENERATED_SPV_DIR}")
message(STATUS "Generated headers dir: ${GENERATED_INCLUDE_DIR}")
