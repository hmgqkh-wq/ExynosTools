# SPDX-License-Identifier: MIT
# cmake/generate_shader_headers.cmake
# Robust shader SPIR-V generation for Vulkan using glslangValidator.
# Detects spirv-opt at configure time (no generator expressions), then
# creates custom commands to produce .spv and .h files.

find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(PYTHON3_EXECUTABLE python3)

if(NOT GLSLANG_VALIDATOR)
    message(FATAL_ERROR "glslangValidator not found. Install glslang-tools or provide glslangValidator in PATH.")
endif()
if(NOT PYTHON3_EXECUTABLE)
    message(FATAL_ERROR "python3 not found. Install Python 3.")
endif()

# Configure-time detection of spirv-opt (no generator expressions)
find_program(SPIRV_OPT_BIN spirv-opt)
if(SPIRV_OPT_BIN)
    message(STATUS "spirv-opt found at: ${SPIRV_OPT_BIN}")
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
        # If spirv-opt exists, we generate raw SPV then optimize into SPV_OPT, then emit header from SPV_OPT
        add_custom_command(
            OUTPUT "${SPV_RAW}" "${SPV_OPT}" "${HEADER_OUT}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling (Vulkan) ${SRC_NAME} -> ${SPV_RAW}"
            COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.2 "-I${SHADER_SRC_DIR}" -o "${SPV_RAW}" "${SRC}"
            COMMAND ${CMAKE_COMMAND} -E echo "Optimizing ${SPV_RAW} -> ${SPV_OPT}"
            COMMAND "${SPIRV_OPT_BIN}" --strip-debug -O "${SPV_RAW}" -o "${SPV_OPT}"
            COMMAND ${CMAKE_COMMAND} -E echo "Emitting header ${HEADER_OUT} from optimized SPV"
            COMMAND ${PYTHON3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py" "${SPV_OPT}" "${HEADER_OUT}" "${SRC_NAME}_spv"
            DEPENDS "${SRC}" "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py"
            COMMENT "Generate optimized SPV and header for ${SRC_NAME}"
            VERBATIM
        )
    else()
        # No spirv-opt: compile SPV and emit header from raw SPV
        add_custom_command(
            OUTPUT "${SPV_RAW}" "${HEADER_OUT}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling (Vulkan) ${SRC_NAME} -> ${SPV_RAW}"
            COMMAND ${GLSLANG_VALIDATOR} -V --target-env vulkan1.2 "-I${SHADER_SRC_DIR}" -o "${SPV_RAW}" "${SRC}"
            COMMAND ${CMAKE_COMMAND} -E echo "Emitting header ${HEADER_OUT} from raw SPV"
            COMMAND ${PYTHON3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py" "${SPV_RAW}" "${HEADER_OUT}" "${SRC_NAME}_spv"
            DEPENDS "${SRC}" "${CMAKE_SOURCE_DIR}/cmake/emit_spv_header.py"
            COMMENT "Generate SPV and header for ${SRC_NAME}"
            VERBATIM
        )
    endif()

    list(APPEND GENERATED_HEADERS "${HEADER_OUT}")
endforeach()

# Custom target to build all generated headers (and SPV). Mark ALL so it's built by default.
add_custom_target(gen_shaders ALL DEPENDS ${GENERATED_HEADERS})

# Make any known targets depend on generated headers so they won't build before headers exist.
foreach(tgt IN ITEMS exynostools xeno_wrapper)
  if(TARGET ${tgt})
    add_dependencies(${tgt} gen_shaders)
  endif()
endforeach()

# Make the generated include dir visible to the compiler
include_directories("${GENERATED_INCLUDE_DIR}")
message(STATUS "Shader source dir: ${SHADER_SRC_DIR}")
message(STATUS "Generated SPV dir: ${GENERATED_SPV_DIR}")
message(STATUS "Generated headers dir: ${GENERATED_INCLUDE_DIR}")
