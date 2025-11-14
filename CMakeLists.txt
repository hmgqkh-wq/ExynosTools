cmake_minimum_required(VERSION 3.18)
project(ExynosTools LANGUAGES C)

# Build basics
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
option(ENABLE_LTO "Enable link-time optimization" ON)
option(SHADERS_REQUIRED "Fail if shader compiler or sources are missing" ON)

# WG size controls for shaders and pipeline
# - WG_SPEC_CONST_ID controls the specialization constant ID used by shaders (matches local_size_x_id)
# - WG_SIZE_DEFAULT sets the default baked-in value (pipeline may override via specialization at runtime)
set(WG_SPEC_CONST_ID "0" CACHE STRING "Specialization constant ID for workgroup size")
set(WG_SIZE_DEFAULT "64" CACHE STRING "Default workgroup size baked into shaders")

# Shader list (keep lowercase filenames consistent with assets/shaders/src)
set(BC_SHADERS bc1 bc2 bc3 bc4 bc5 bc6h bc7)

# Dependencies
find_program(GLSLC_EXECUTABLE glslc REQUIRED)
message(STATUS "Found glslc: ${GLSLC_EXECUTABLE}")

# Generator for header from SPIR-V (creates <var>.h with <var> and <var>_len)
set(GEN_HEADER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/generate_shader_header.cmake")
if(NOT EXISTS "${GEN_HEADER_SCRIPT}")
  message(FATAL_ERROR "Missing required file: ${GEN_HEADER_SCRIPT}")
endif()

# Generated directories
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(SHADER_HEADERS_DIR "${GENERATED_DIR}/shaders")
file(MAKE_DIRECTORY "${GENERATED_DIR}")
file(MAKE_DIRECTORY "${SHADER_HEADERS_DIR}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")

# Helper: compile a single shader to SPIR-V then to a C header
function(compile_shader_to_header SHADER_SOURCE HEADER_OUTPUT VARIABLE_NAME_LOWER DEFINES)
  get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
  set(SPIRV_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")

  if(SHADERS_REQUIRED AND NOT EXISTS "${SHADER_SOURCE}")
    message(FATAL_ERROR "Shader source required but missing: ${SHADER_SOURCE}")
  endif()

  add_custom_command(
    OUTPUT ${SPIRV_OUTPUT}
    COMMAND ${GLSLC_EXECUTABLE}
            -x glsl
            -I${CMAKE_SOURCE_DIR}/assets/shaders/src
            ${DEFINES}
            ${SHADER_SOURCE}
            -o ${SPIRV_OUTPUT}
    DEPENDS ${SHADER_SOURCE}
    COMMENT "Compiling ${SHADER_SOURCE} -> ${SPIRV_OUTPUT}"
    VERBATIM
  )

  add_custom_command(
    OUTPUT ${HEADER_OUTPUT}
    COMMAND ${CMAKE_COMMAND}
            -DSPIRV_FILE=${SPIRV_OUTPUT}
            -DHEADER_FILE=${HEADER_OUTPUT}
            -DVAR_NAME=${VARIABLE_NAME_LOWER}
            -P ${GEN_HEADER_SCRIPT}
    DEPENDS ${SPIRV_OUTPUT} ${GEN_HEADER_SCRIPT}
    COMMENT "Generating header ${HEADER_OUTPUT}"
    VERBATIM
  )
endfunction()

# Build shader headers
set(GENERATED_HEADERS "")
foreach(s IN LISTS BC_SHADERS)
  set(SRC "${CMAKE_SOURCE_DIR}/assets/shaders/src/${s}.comp")
  set(HDR "${SHADER_HEADERS_DIR}/${s}_shader.h")
  set(VARNAME_LOWER "${s}_shader_spv")

  # Shader compile defines:
  # -DWORKGROUP_CONST_ID=<id> for local_size_x_id
  # -DWG_SIZE_DEFAULT=<n> to bake a default into bc_common.glsl
  set(DEF "-DWORKGROUP_CONST_ID=${WG_SPEC_CONST_ID} -DWG_SIZE_DEFAULT=${WG_SIZE_DEFAULT}")

  compile_shader_to_header("${SRC}" "${HDR}" "${VARNAME_LOWER}" "${DEF}")
  list(APPEND GENERATED_HEADERS "${HDR}")
endforeach()

# Also include the shared header so glslc can resolve #include "bc_common.glsl"
# (Note: bc_common.glsl is used by the shaders; it is not compiled directly.)
if(SHADERS_REQUIRED AND NOT EXISTS "${CMAKE_SOURCE_DIR}/assets/shaders/src/bc_common.glsl}")
  message(FATAL_ERROR "Missing shared shader header: assets/shaders/src/bc_common.glsl")
endif()

add_custom_target(generated_shaders ALL DEPENDS ${GENERATED_HEADERS})

# Library sources (adapt to your repo)
set(SRC_FILES
  src/xeno_wrapper.c
  src/xeno_wrapper.h
  src/bc_emulate.c
  src/bc_emulate.h
  src/logging.c
  src/perf_conf.c
  src/app_profile.c
  src/hud.c
)

# Build shared library
add_library(xeno_wrapper SHARED
  ${SRC_FILES}
  ${GENERATED_HEADERS}
)

add_dependencies(xeno_wrapper generated_shaders)

target_include_directories(xeno_wrapper
  PRIVATE
    ${GENERATED_DIR}
    ${SHADER_HEADERS_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)
target_link_libraries(xeno_wrapper PRIVATE Threads::Threads vulkan dl)

# LTO
if(ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_msg)
  if(ipo_supported)
    set_target_properties(xeno_wrapper PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endif()

# Public header and naming
set_target_properties(xeno_wrapper PROPERTIES
  OUTPUT_NAME "xeno_wrapper"
  PREFIX "lib"
  PUBLIC_HEADER "src/xeno_wrapper.h"
)

# Compile-time visibility of WG settings (optional)
target_compile_definitions(xeno_wrapper PRIVATE
  WG_SPEC_CONST_ID=${WG_SPEC_CONST_ID}
  WG_SIZE_DEFAULT=${WG_SIZE_DEFAULT}
)

# Install (adjust paths to your packaging layout)
install(TARGETS xeno_wrapper
  LIBRARY DESTINATION usr/lib
  PUBLIC_HEADER DESTINATION usr/include
)
