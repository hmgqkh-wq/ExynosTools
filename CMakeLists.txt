cmake_minimum_required(VERSION 3.16)

project(ExynosTools C)

# Allow overriding these from the environment or CI if needed
if(NOT DEFINED EXYNOSTOOLS_STAGING_POOL_SIZE)
  set(EXYNOSTOOLS_STAGING_POOL_SIZE 1048576)
endif()

if(NOT DEFINED XCLIPSE_940_OPTIMIZE)
  set(XCLIPSE_940_OPTIMIZE 1)
endif()

# Build type defaults
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Common compile options
add_compile_options(
  -Wall
  -Wextra
  -Wno-unused-parameter
  $<$<CONFIG:Release>:-O3>
)

# Export selected defines to the compiler
add_compile_definitions(
  EXYNOSTOOLS_STAGING_POOL_SIZE=${EXYNOSTOOLS_STAGING_POOL_SIZE}
  XCLIPSE_940_OPTIMIZE=${XCLIPSE_940_OPTIMIZE}
)

# Directories
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(GENERATED_SPV_DIR "${CMAKE_BINARY_DIR}/generated_spv") # matches build/generated_spv seen in CI

# Ensure include directories: project's include, src, and generated SPV output
include_directories(
  "${INCLUDE_DIR}"
  "${SRC_DIR}"
  "${GENERATED_SPV_DIR}"
)

# List of source files. Edit to match your repo layout if needed.
set(PROJECT_SOURCES
  ${SRC_DIR}/main.c
  ${SRC_DIR}/logging.c
  ${SRC_DIR}/xeno_log_stream.c
  ${SRC_DIR}/bc_emulate.c
  ${SRC_DIR}/emulate.c
  ${SRC_DIR}/create_common_layouts_and_pipelines.c
  ${SRC_DIR}/drivers/xclipse/async.c
  ${SRC_DIR}/app_profile.c
  ${SRC_DIR}/detect.c
  # add other sources here as required
)

# Only add sources that exist so configure won't fail on partial checkouts
set(EXISTING_SOURCES "")
foreach(f ${PROJECT_SOURCES})
  if(EXISTS ${f})
    list(APPEND EXISTING_SOURCES ${f})
  else()
    message(STATUS "Notice: source not found at configure time: ${f}")
  endif()
endforeach()

add_executable(ExynosTools ${EXISTING_SOURCES})

# Link to Vulkan if available
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(VULKAN_PKG QUIET vulkan)
  if(VULKAN_PKG_FOUND)
    target_include_directories(ExynosTools PRIVATE ${VULKAN_PKG_INCLUDE_DIRS})
    target_link_libraries(ExynosTools PRIVATE ${VULKAN_PKG_LDFLAGS})
  else()
    find_library(VULKAN_LIB NAMES vulkan Vulkan)
    if(VULKAN_LIB)
      target_link_libraries(ExynosTools PRIVATE ${VULKAN_LIB})
    endif()
  endif()
else()
  find_library(VULKAN_LIB NAMES vulkan Vulkan)
  if(VULKAN_LIB)
    target_link_libraries(ExynosTools PRIVATE ${VULKAN_LIB})
  endif()
endif()

# Threads
find_package(Threads)
if(THREADS_FOUND)
  target_link_libraries(ExynosTools PRIVATE Threads::Threads)
endif()

# If you have a shader-generator custom command that writes into ${GENERATED_SPV_DIR},
# make sure it runs before compiling ExynosTools. Example commented snippet:
#
# add_custom_command(
#   OUTPUT ${GENERATED_SPV_DIR}/bc1_shader.h ${GENERATED_SPV_DIR}/bc2_shader.h ...
#   COMMAND python3 ${CMAKE_SOURCE_DIR}/assets/shaders/emit_spv_header.py --out ${GENERATED_SPV_DIR}
#   DEPENDS ${CMAKE_SOURCE_DIR}/assets/shaders/*.comp ${CMAKE_SOURCE_DIR}/assets/shaders/*.glsl
# )
# add_custom_target(gen_spv_headers DEPENDS ${GENERATED_SPV_DIR}/bc1_shader.h ...)
# add_dependencies(ExynosTools gen_spv_headers)

message(STATUS "ExynosTools configured with include dirs: ${INCLUDE_DIR}, ${SRC_DIR}, ${GENERATED_SPV_DIR}")
message(STATUS "Sources compiled: ${EXISTING_SOURCES}")

target_compile_definitions(ExynosTools PRIVATE
  EXYNOSTOOLS_STAGING_POOL_SIZE=${EXYNOSTOOLS_STAGING_POOL_SIZE}
  XCLIPSE_940_OPTIMIZE=${XCLIPSE_940_OPTIMIZE}
)
