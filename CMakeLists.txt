cmake_minimum_required(VERSION 3.18)
project(ExynosTools LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output dirs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/src
)

# Source aggregation
file(GLOB_RECURSE EXYNOSTOOLS_SOURCES
    src/*.cpp
    src/*.c
)

add_executable(exynostools ${EXYNOSTOOLS_SOURCES})

# Vulkan
find_package(Vulkan REQUIRED)
target_link_libraries(exynostools PRIVATE Vulkan::Vulkan)

# Shader compile
set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/src)
set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

# Only compile .comp files
set(SHADER_SOURCES
    bc1.comp
    bc2.comp
    bc3.comp
    bc4.comp
    bc5.comp
    bc6h.comp
    bc7.comp
)

foreach(SHADER_FILE ${SHADER_SOURCES})
    set(INPUT_FILE ${SHADER_SRC_DIR}/${SHADER_FILE})
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
    set(OUTPUT_FILE ${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv)

    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${CMAKE_COMMAND} -E echo "Compiling shader: ${SHADER_FILE}"
        COMMAND glslc ${INPUT_FILE} -o ${OUTPUT_FILE}
        DEPENDS ${INPUT_FILE} ${SHADER_SRC_DIR}/bc_common.glsl
        COMMENT "Compiling ${SHADER_FILE} to SPIR-V"
        VERBATIM
    )

    list(APPEND COMPILED_SHADERS ${OUTPUT_FILE})
endforeach()

add_custom_target(generated_shaders ALL DEPENDS ${COMPILED_SHADERS})
add_dependencies(exynostools generated_shaders)

# Install/packaging (optional)
install(DIRECTORY ${SHADER_OUTPUT_DIR}/ DESTINATION shaders)
install(TARGETS exynostools RUNTIME DESTINATION bin)
