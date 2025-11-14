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

# Header generation from SPIR-V
set(SHADER_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_HEADER_DIR})

function(spv_to_header SPV_FILE HEADER_FILE SYMBOL_NAME)
    add_custom_command(
        OUTPUT ${HEADER_FILE}
        COMMAND ${CMAKE_COMMAND}
            -DINPUT=${SPV_FILE}
            -DOUTPUT=${HEADER_FILE}
            -DSYMBOL=${SYMBOL_NAME}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/spv_to_header.cmake
        DEPENDS ${SPV_FILE}
        COMMENT "Generating header ${HEADER_FILE} from ${SPV_FILE}"
        VERBATIM
    )
    set(GENERATED_HEADERS ${GENERATED_HEADERS} ${HEADER_FILE} PARENT_SCOPE)
endfunction()

set(SHADER_NAMES bc1 bc2 bc3 bc4 bc5 bc6h bc7)

foreach(NAME ${SHADER_NAMES})
    set(SPV ${SHADER_OUTPUT_DIR}/${NAME}.spv)
    set(HDR ${SHADER_HEADER_DIR}/${NAME}_shader.h)
    set(SYM ${NAME}_spv)
    spv_to_header(${SPV} ${HDR} ${SYM})
endforeach()

add_custom_target(generated_shader_headers ALL DEPENDS ${GENERATED_HEADERS})
add_dependencies(exynostools generated_shader_headers)
target_include_directories(exynostools PRIVATE ${SHADER_HEADER_DIR})

# Optional install
install(DIRECTORY ${SHADER_OUTPUT_DIR}/ DESTINATION shaders)
install(TARGETS exynostools RUNTIME DESTINATION bin)
