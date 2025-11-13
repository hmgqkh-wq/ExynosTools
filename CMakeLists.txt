cmake_minimum_required(VERSION 3.15)

project(ExynosTools LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(XCLIPSE_ENABLE_ANDROID_LOG "Enable Android logging" OFF)
option(BUILD_WITH_THREAD_SANITIZER "Build with thread sanitizer (for debugging only)" OFF)

# Prefer glslc if available, but do not hard-fail the configure step if it's missing.
find_program(GLSLC_EXECUTABLE glslc)
if(NOT GLSLC_EXECUTABLE)
  message(STATUS "glslc not found in PATH; shader compilation will be skipped and fallback headers generated")
else()
  message(STATUS "Found glslc: ${GLSLC_EXECUTABLE}")
endif()

# Shader header generator script (always present)
set(GEN_HEADER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/generate_shader_header.cmake")
if(NOT EXISTS "${GEN_HEADER_SCRIPT}")
  message(FATAL_ERROR "Missing required file: ${GEN_HEADER_SCRIPT}")
endif()

# Helper function: compile a GLSL shader to SPIR-V and then to a C header.
# If glslc is not found or shader source missing, a fallback header (empty array) will be generated.
function(compile_shader_to_header SHADER_SOURCE HEADER_OUTPUT VARIABLE_NAME)
  get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
  set(SPIRV_OUTPUT "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shaders")
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/shaders")

  if(EXISTS "${SHADER_SOURCE}" AND GLSLC_EXECUTABLE)
    # Normal flow: compile shader to SPIR-V, then run generator to produce header.
    add_custom_command(
      OUTPUT ${SPIRV_OUTPUT}
      COMMAND ${GLSLC_EXECUTABLE} -I${CMAKE_SOURCE_DIR}/assets/shaders/src ${SHADER_SOURCE} -o ${SPIRV_OUTPUT}
      DEPENDS ${SHADER_SOURCE} ${CMAKE_SOURCE_DIR}/assets/shaders/src/bc_common.glsl
      COMMENT "Compiling ${SHADER_SOURCE} to SPIR-V"
      VERBATIM
    )

    add_custom_command(
      OUTPUT ${HEADER_OUTPUT}
      COMMAND ${CMAKE_COMMAND} -DSPIRV_FILE=${SPIRV_OUTPUT} -DHEADER_FILE=${HEADER_OUTPUT} -DVAR_NAME=${VARIABLE_NAME} -P ${GEN_HEADER_SCRIPT}
      DEPENDS ${SPIRV_OUTPUT} ${GEN_HEADER_SCRIPT}
      COMMENT "Generating C header ${HEADER_OUTPUT} from ${SPIRV_OUTPUT}"
      VERBATIM
    )
  else()
    # Fallback: generate a harmless empty uint32_t array header so build continues.
    add_custom_command(
      OUTPUT ${HEADER_OUTPUT}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated/shaders
      COMMAND ${CMAKE_COMMAND} -DSPIRV_FILE=__MISSING__ -DHEADER_FILE=${HEADER_OUTPUT} -DVAR_NAME=${VARIABLE_NAME} -P ${GEN_HEADER_SCRIPT}
      DEPENDS ${GEN_HEADER_SCRIPT}
      COMMENT "Generating fallback header ${HEADER_OUTPUT} (glslc or shader missing)"
      VERBATIM
    )
  endif()
endfunction()

# Where generated headers will go
set(SHADER_HEADERS_DIR "${CMAKE_BINARY_DIR}/generated/shaders")
file(MAKE_DIRECTORY ${SHADER_HEADERS_DIR})

# Declare shader conversions (these produce headers as outputs)
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

# Ensure headers are built before the library by introducing an explicit target
add_custom_target(generated_shaders DEPENDS
  ${SHADER_HEADERS_DIR}/bc4_shader.h
  ${SHADER_HEADERS_DIR}/bc5_shader.h
)

# Collect sources explicitly to avoid missing files causing glob surprises
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

add_library(xeno_wrapper SHARED
  ${SRC_FILES}
  ${SHADER_HEADERS_DIR}/bc4_shader.h
  ${SHADER_HEADERS_DIR}/bc5_shader.h
)

add_dependencies(xeno_wrapper generated_shaders)

target_compile_definitions(xeno_wrapper PRIVATE $<$<BOOL:${XCLIPSE_ENABLE_ANDROID_LOG}>:XCLIPSE_ANDROID_LOG>)
if(BUILD_WITH_THREAD_SANITIZER)
  target_compile_options(xeno_wrapper PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
  target_link_options(xeno_wrapper PRIVATE -fsanitize=thread)
endif()

find_package(Threads REQUIRED)
if(UNIX)
  target_link_libraries(xeno_wrapper PRIVATE dl Threads::Threads vulkan)
endif()

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
