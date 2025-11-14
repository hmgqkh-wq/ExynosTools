cmake_minimum_required(VERSION 3.18)

project(ExynosTools LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(XCLIPSE_ENABLE_ANDROID_LOG "Enable Android logging" OFF)
option(BUILD_WITH_THREAD_SANITIZER "Build with thread sanitizer (for debugging only)" OFF)
option(ENABLE_LTO "Enable link-time optimization" ON)
option(SHADERS_REQUIRED "Fail configure if shader compiler or sources are missing" ON)
option(ENABLE_BINDLESS "Enable VK_EXT_descriptor_indexing usage" ON)
option(ENABLE_RAYTRACING "Enable VK_KHR_ray_tracing_pipeline path" ON)

find_program(GLSLC_EXECUTABLE glslc REQUIRED)
message(STATUS "Found glslc: ${GLSLC_EXECUTABLE}")

set(GEN_HEADER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/generate_shader_header.cmake")
if(NOT EXISTS "${GEN_HEADER_SCRIPT}")
  message(FATAL_ERROR "Missing required file: ${GEN_HEADER_SCRIPT}")
endif()

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(SHADER_HEADERS_DIR "${GENERATED_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_HEADERS_DIR})
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")

function(compile_shader_to_header SHADER_SOURCE HEADER_OUTPUT VARIABLE_NAME_LOWER DEFINES)
  get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
  set(SPIRV_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")

  if(NOT EXISTS "${SHADER_SOURCE}")
    message(FATAL_ERROR "Shader source required but missing: ${SHADER_SOURCE}")
  endif()

  add_custom_command(
    OUTPUT ${SPIRV_OUTPUT}
    COMMAND ${GLSLC_EXECUTABLE}
            -I${CMAKE_SOURCE_DIR}/assets/shaders/src
            ${DEFINES}
            ${SHADER_SOURCE}
            -o ${SPIRV_OUTPUT}
    DEPENDS ${SHADER_SOURCE}
    COMMENT "Compiling ${SHADER_SOURCE} to SPIR-V"
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

set(BC_SHADERS bc1 bc3 bc4 bc5 bc6h bc7)

set(GENERATED_HEADERS "")
foreach(s IN LISTS BC_SHADERS)
  set(SRC "${CMAKE_SOURCE_DIR}/assets/shaders/src/${s}.comp")
  set(HDR "${SHADER_HEADERS_DIR}/${s}_shader.h")
  set(VARNAME_LOWER "${s}_shader_spv")
  set(DEF "-DWORKGROUP_CONST_ID=0")
  if(ENABLE_BINDLESS)
    set(DEF "${DEF} -DENABLE_BINDLESS=1")
  endif()
  compile_shader_to_header("${SRC}" "${HDR}" "${VARNAME_LOWER}" "${DEF}")
  list(APPEND GENERATED_HEADERS "${HDR}")
endforeach()

add_custom_target(generated_shaders ALL DEPENDS ${GENERATED_HEADERS})

include_directories(${GENERATED_DIR} ${SHADER_HEADERS_DIR} ${CMAKE_SOURCE_DIR}/src)

set(SRC_FILES
  src/xeno_wrapper.c
  src/xeno_wrapper.h
  src/bc_emulate.c
  src/bc_emulate.h
  src/features_patch.c
  src/detect.c
  src/perf_conf.c
  src/logging.c
  src/app_profile.c
  src/hud.c
  src/rt_path.c
  src/rt_path.h
)

add_library(xeno_wrapper SHARED
  ${SRC_FILES}
  ${GENERATED_HEADERS}
)

add_dependencies(xeno_wrapper generated_shaders)

target_include_directories(xeno_wrapper PRIVATE ${GENERATED_DIR} ${SHADER_HEADERS_DIR} ${CMAKE_SOURCE_DIR}/src)
find_package(Threads REQUIRED)
target_link_libraries(xeno_wrapper PRIVATE dl Threads::Threads vulkan)

if(ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_msg)
  if(ipo_supported)
    set_target_properties(xeno_wrapper PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endif()

set_target_properties(xeno_wrapper PROPERTIES
  OUTPUT_NAME "xeno_wrapper"
  PREFIX "lib"
  PUBLIC_HEADER "src/xeno_wrapper.h"
)

target_compile_definitions(xeno_wrapper PRIVATE
  $<$<BOOL:${XCLIPSE_ENABLE_ANDROID_LOG}>:XCLIPSE_ANDROID_LOG>
  $<$<BOOL:${ENABLE_BINDLESS}>:ENABLE_BINDLESS>
  $<$<BOOL:${ENABLE_RAYTRACING}>:ENABLE_RAYTRACING>
)

install(TARGETS xeno_wrapper
  LIBRARY DESTINATION usr/lib
  PUBLIC_HEADER DESTINATION usr/include
)

install(FILES usr/share/meta.json DESTINATION usr/share)
install(FILES etc/exynostools/performance_mode.conf DESTINATION etc/exynostools)
