cmake_minimum_required(VERSION 3.15)

project(ExynosTools LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(XCLIPSE_ENABLE_ANDROID_LOG "Enable Android logging" OFF)
option(BUILD_WITH_THREAD_SANITIZER "Build with thread sanitizer (for debugging only)" OFF)

# Find required tools for shader compilation
find_program(GLSLC_EXECUTABLE glslc REQUIRED)
find_program(XXD_EXECUTABLE xxd) # used by fallback header generator (preferred on CI)

# Shader compilation function
function(compile_shader_to_header SHADER_SOURCE HEADER_OUTPUT VARIABLE_NAME)
    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
    set(SPIRV_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
    
    # Create shaders directory
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/shaders")
    
    # Compile GLSL to SPIR-V
    add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND ${GLSLC_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/assets/shaders/src ${SHADER_SOURCE} -o ${SPIRV_OUTPUT}
        DEPENDS ${SHADER_SOURCE} ${CMAKE_SOURCE_DIR}/assets/shaders/src/bc_common.glsl
        COMMENT "Compiling ${SHADER_SOURCE} to SPIR-V"
        VERBATIM
    )
    
    # Generate C header from SPIR-V using a robust generator script (preferred)
    add_custom_command(
        OUTPUT ${HEADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -D SPIRV_FILE=${SPIRV_OUTPUT} -D HEADER_FILE=${HEADER_OUTPUT} -D VAR_NAME=${VARIABLE_NAME} -P ${CMAKE_CURRENT_SOURCE_DIR}/generate_shader_header.cmake
        DEPENDS ${SPIRV_OUTPUT}
        COMMENT "Generating C header from ${SPIRV_OUTPUT}"
        VERBATIM
    )
endfunction()

# Create the generate_shader_header.cmake script (overwrite safe)
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/generate_shader_header.cmake "
# Small robust generator for SPIR-V -> uint32_t array header
if(NOT DEFINED SPIRV_FILE)
  message(FATAL_ERROR \"SPIRV_FILE not set\")
endif()
if(NOT DEFINED HEADER_FILE)
  message(FATAL_ERROR \"HEADER_FILE not set\")
endif()
if(NOT DEFINED VAR_NAME)
  message(FATAL_ERROR \"VAR_NAME not set\")
endif()

file(TO_CMAKE_PATH \"${SPIRV_FILE}\" __spv_in)
file(TO_CMAKE_PATH \"${HEADER_FILE}\" __hdr_out)

if(NOT EXISTS \"${__spv_in}\")
  message(FATAL_ERROR \"SPIR-V file not found: ${__spv_in}\")
endif()

# Read binary
file(READ \"${__spv_in}\" _SPV HEX RAW)
# CMake file(READ) returns raw bytes in variable _SPV; convert to hex pairs
string(LENGTH \"${_SPV}\" _len_bytes)
math(EXPR _num_words \"(${_len_bytes} + 3) / 4\")

set(_words \"\")
foreach(i RANGE 0 ${_num_words} 1)
  math(EXPR off \"${i} * 4\")
  set(_w 0)
  foreach(b RANGE 0 3)
    math(EXPR idx \"${off} + ${b}\")
    if(idx GREATER_EQUAL _len_bytes)
      break()
    endif()
    string(SUBSTRING \"${_SPV}\" ${idx} 1 _byte)
    # get numeric value of byte; use character code via execute_process printf
    execute_process(COMMAND /bin/printf \"%d\" \"'${_byte}\" OUTPUT_VARIABLE _bval OUTPUT_STRIP_TRAILING_WHITESPACE)
    math(EXPR _w \"${_w} + (${_bval} << (${b} * 8))\")
  endforeach()
  string(APPEND _words \"0x${_w}, \")
endforeach()

file(WRITE \"${__hdr_out}\" \"#pragma once\\n#include <stdint.h>\\nstatic const uint32_t ${VAR_NAME}[] = { ${_words} };\\nstatic const unsigned int ${VAR_NAME}_len = sizeof(${VAR_NAME});\\n\")
")

# Compile shaders to headers
set(SHADER_HEADERS_DIR "${CMAKE_BINARY_DIR}/generated/shaders")
file(MAKE_DIRECTORY ${SHADER_HEADERS_DIR})

compile_shader_to_header(
    "${CMAKE_SOURCE_DIR}/assets/shaders/src/bc4.comp"
    "${SHADER_HEADERS_DIR}/bc4_shader.h"
    "bc4_shader_spv"
)

compile_shader_to_header(
    "${CMAKE_SOURCE_DIR}/assets/shaders/src/bc5.comp"
    "${SHADER_HEADERS_DIR}/bc5_shader.h"
    "bc5_shader_spv"
)

# Library sources
file(GLOB_RECURSE EXYNOS_TOOL_SOURCES src/*.c src/*.h)
add_library(xeno_wrapper SHARED
    ${EXYNOS_TOOL_SOURCES}
    ${SHADER_HEADERS_DIR}/bc4_shader.h
    ${SHADER_HEADERS_DIR}/bc5_shader.h
)

target_compile_definitions(xeno_wrapper PRIVATE $<$<BOOL:${XCLIPSE_ENABLE_ANDROID_LOG}>:XCLIPSE_ANDROID_LOG>)
if(BUILD_WITH_THREAD_SANITIZER)
  target_compile_options(xeno_wrapper PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
  target_link_options(xeno_wrapper PRIVATE -fsanitize=thread)
endif()

find_package(Threads REQUIRED)
if(UNIX)
    target_link_libraries(xeno_wrapper PRIVATE dl Threads::Threads vulkan)
endif()

# Add include directory for generated shader headers
target_include_directories(xeno_wrapper PRIVATE ${SHADER_HEADERS_DIR} ${CMAKE_SOURCE_DIR}/src)

set_target_properties(xeno_wrapper PROPERTIES
    OUTPUT_NAME "xeno_wrapper"
    PREFIX "lib"
    PUBLIC_HEADER "src/xeno_wrapper.h"
)

install(TARGETS xeno_wrapper
    LIBRARY DESTINATION usr/lib
    PUBLIC_HEADER DESTINATION usr/include
)

install(FILES usr/share/meta.json DESTINATION usr/share)
install(FILES etc/exynostools/performance_mode.conf DESTINATION etc/exynostools)
install(DIRECTORY vendor DESTINATION .)
install(DIRECTORY profiles DESTINATION .)
