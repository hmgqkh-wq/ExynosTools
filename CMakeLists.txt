cmake_minimum_required(VERSION 3.15)

project(ExynosTools LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(XCLIPSE_ENABLE_ANDROID_LOG "Enable Android logging" OFF)
option(BUILD_WITH_THREAD_SANITIZER "Build with thread sanitizer (for debugging only)" OFF)
option(SHADERS_REQUIRED "Fail configure if shader compiler or sources are missing" OFF)

# Prefer glslc if available
find_program(GLSLC_EXECUTABLE glslc)
if(NOT GLSLC_EXECUTABLE)
  message(STATUS "glslc not found in PATH; shader compilation will be skipped and fallback headers generated")
else()
  message(STATUS "Found glslc: ${GLSLC_EXECUTABLE}")
endif()

# Generator script path (must exist)
set(GEN_HEADER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/generate_shader_header.cmake")
if(NOT EXISTS "${GEN_HEADER_SCRIPT}")
  message(FATAL_ERROR "Missing required file: ${GEN_HEADER_SCRIPT}")
endif()

# Generated directories (align with CI logs)
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(SHADER_HEADERS_DIR "${GENERATED_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_HEADERS_DIR})

# Helper: compile shader -> SPIR-V -> C header, or emit fallback header
function(compile_shader_to_header SHADER_SOURCE HEADER_OUTPUT VARIABLE_NAME_LOWER)
  get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
  set(SPIRV_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")
  file(MAKE_DIRECTORY "${SHADER_HEADERS_DIR}")

  if(EXISTS "${SHADER_SOURCE}")
    if(GLSLC_EXECUTABLE)
      add_custom_command(
        OUTPUT ${SPIRV_OUTPUT}
        COMMAND ${GLSLC_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/assets/shaders/src ${SHADER_SOURCE} -o ${SPIRV_OUTPUT}
        DEPENDS ${SHADER_SOURCE} ${CMAKE_SOURCE_DIR}/assets/shaders/src/bc_common.glsl
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
        COMMENT "Generating C header ${HEADER_OUTPUT} from ${SPIRV_OUTPUT}"
        VERBATIM
      )
    else()
      if(SHADERS_REQUIRED)
        message(FATAL_ERROR "Shader compiler not found but SHADERS_REQUIRED=ON")
      endif()
      add_custom_command(
        OUTPUT ${HEADER_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_HEADERS_DIR}
        COMMAND ${CMAKE_COMMAND}
                -DSPIRV_FILE=__MISSING__
                -DHEADER_FILE=${HEADER_OUTPUT}
                -DVAR_NAME=${VARIABLE_NAME_LOWER}
                -P ${GEN_HEADER_SCRIPT}
        DEPENDS ${GEN_HEADER_SCRIPT}
        COMMENT "Generating fallback header ${HEADER_OUTPUT} (glslc missing)"
        VERBATIM
      )
    endif()
  else()
    if(SHADERS_REQUIRED)
      message(FATAL_ERROR "Shader source ${SHADER_SOURCE} required but missing")
    endif()
    add_custom_command(
      OUTPUT ${HEADER_OUTPUT}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_HEADERS_DIR}
      COMMAND ${CMAKE_COMMAND}
              -DSPIRV_FILE=__MISSING__
              -DHEADER_FILE=${HEADER_OUTPUT}
              -DVAR_NAME=${VARIABLE_NAME_LOWER}
              -P ${GEN_HEADER_SCRIPT}
      DEPENDS ${GEN_HEADER_SCRIPT}
      COMMENT "Generating fallback header ${HEADER_OUTPUT} (shader source missing)"
      VERBATIM
    )
  endif()
endfunction()

# Shaders referenced by sources — keep these in sync with includes in bc_emulate.c
set(BC_SHADERS
  bc1
  bc3
  bc4
  bc5
  bc6h
  bc7
)

# Request generation and collect header outputs
set(GENERATED_HEADERS "")
foreach(s IN LISTS BC_SHADERS)
  set(SRC "${CMAKE_SOURCE_DIR}/assets/shaders/src/${s}.comp")
  set(HDR "${SHADER_HEADERS_DIR}/${s}_shader.h")
  set(VARNAME_LOWER "${s}_shader_spv")
  compile_shader_to_header("${SRC}" "${HDR}" "${VARNAME_LOWER}")
  list(APPEND GENERATED_HEADERS "${HDR}")
endforeach()

# Build generated headers before everything else
add_custom_target(generated_shaders ALL DEPENDS ${GENERATED_HEADERS})
set_target_properties(generated_shaders PROPERTIES EXCLUDE_FROM_ALL FALSE)

# Expose generated include dirs
include_directories(${GENERATED_DIR} ${SHADER_HEADERS_DIR})

# Explicit source list
set(SRC_FILES
  src/xeno_wrapper.c
  src/bc_emulate.c
  src/features_patch.c
  src/detect.c
  src/perf_conf.c
  src/logging.c
  src/app_profile.c
  src/hud.c
)

# Create library target
add_library(xeno_wrapper SHARED
  ${SRC_FILES}
  ${GENERATED_HEADERS}
)

add_dependencies(xeno_wrapper generated_shaders)

target_include_directories(xeno_wrapper PRIVATE ${GENERATED_DIR} ${SHADER_HEADERS_DIR} ${CMAKE_SOURCE_DIR}/src)

target_compile_definitions(xeno_wrapper PRIVATE $<$<BOOL:${XCLIPSE_ENABLE_ANDROID_LOG}>:XCLIPSE_ANDROID_LOG>)
if(BUILD_WITH_THREAD_SANITIZER)
  target_compile_options(xeno_wrapper PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
  target_link_options(xeno_wrapper PRIVATE -fsanitize=thread)
endif()

find_package(Threads REQUIRED)
if(UNIX)
  target_link_libraries(xeno_wrapper PRIVATE dl Threads::Threads vulkan)
endif()

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

# Removed: install(DIRECTORY vendor DESTINATION .)
# If you later add a vendor directory, re-enable this line.
