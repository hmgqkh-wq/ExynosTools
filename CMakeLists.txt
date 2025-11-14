cmake_minimum_required(VERSION 3.18)
project(ExynosTools LANGUAGES C)

# Enforce Xclipse 940 tuning as a permanent rule for compile-time optimization choices.
# This sets a preprocessor flag consumed by C sources to select Xclipse-940 specific paths.
add_compile_definitions(XCLIPSE_940_OPTIMIZE=1)

# Basic compiler flags (optimize for release by default; user can override)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Allow user to override these if needed for debugging
if(NOT DEFINED EXYNOSTOOLS_STAGING_POOL_SIZE)
  set(EXYNOSTOOLS_STAGING_POOL_SIZE $((1 << 20)) CACHE STRING "Default staging pool size bytes (Xclipse 940 tuned)")
endif()

# Project layout helpers - include our cmake snippets
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Include helper files (idempotent) - these files must exist as provided earlier:
# - cmake/add_project_includes.cmake
# - cmake/generate_shader_headers.cmake
include(cmake/add_project_includes.cmake)
include(cmake/generate_shader_headers.cmake)

# Ensure generated headers are created before the main target builds
add_dependencies(${PROJECT_NAME} gen_shaders) # harmless if target missing; CMake will warn if used before target exists

# Find Vulkan SDK / development headers
find_package(Vulkan REQUIRED)

if(NOT Vulkan_FOUND)
  message(FATAL_ERROR "Vulkan not found. Install the Vulkan SDK or libvulkan-dev for your platform.")
endif()

# Source files (explicit list preferred for reproducibility)
# Adjust filenames to match your repo; this list covers typical files seen in logs.
set(EXYNOSTOOLS_SOURCES
  src/main.c
  src/bc_emulate.c
  src/detect.c
  src/app_profile.c
  src/drivers/xclipse/async.c
  src/drivers/xclipse/other_driver_files.c
  src/logging.c
  src/bc_helpers.c
)

# Filter out missing files gracefully to avoid configure failure in forks
set(EXYNOSTOOLS_EXISTING_SOURCES "")
foreach(f ${EXYNOSTOOLS_SOURCES})
  if(EXISTS "${CMAKE_SOURCE_DIR}/${f}")
    list(APPEND EXYNOSTOOLS_EXISTING_SOURCES ${f})
  else()
    message(STATUS "Skipping missing source (not in repo copy): ${f}")
  endif()
endforeach()

add_executable(${PROJECT_NAME}
  ${EXYNOSTOOLS_EXISTING_SOURCES}
)

# Global include dirs populated by cmake/add_project_includes.cmake
# But also ensure target has them explicitly for modern CMake usage
target_include_directories(${PROJECT_NAME} PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/build_generated_spv"
)

# Link Vulkan
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)

# Compiler warnings and sanitizers (conservative - keep code portable)
target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall -Wextra -Wno-unused-parameter
)

# Enable visibility and optimization suitable for mobile GPU tuning (Xclipse 940)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(${PROJECT_NAME} PRIVATE -O3 -march=native)
endif()

# Add a define exposing the staging pool default to C code
target_compile_definitions(${PROJECT_NAME} PRIVATE
  EXYNOSTOOLS_STAGING_POOL_SIZE=${EXYNOSTOOLS_STAGING_POOL_SIZE}
)

# Make sure generated headers are generated before compiling sources that include them.
# The gen_shaders custom target is created by cmake/generate_shader_headers.cmake.
if(TARGET gen_shaders)
  add_dependencies(${PROJECT_NAME} gen_shaders)
endif()

# Installation (optional)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# Provide a convenience target to run a local microbenchmark harness (if present)
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/benchmark_xclipse.c")
  add_executable(exynostools_bench tools/benchmark_xclipse.c)
  target_link_libraries(exynostools_bench PUBLIC Vulkan::Vulkan)
  target_include_directories(exynostools_bench PUBLIC "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src")
  add_dependencies(exynostools_bench gen_shaders)
endif()

# Diagnostics: print a small summary focused on Xclipse 940 tuning
message(STATUS "ExynosTools configured (Xclipse 940 optimized). Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Generated headers dir: ${CMAKE_SOURCE_DIR}/include")
message(STATUS "Staging pool default size (bytes): ${EXYNOSTOOLS_STAGING_POOL_SIZE}")
