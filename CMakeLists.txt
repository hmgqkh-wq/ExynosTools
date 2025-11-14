cmake_minimum_required(VERSION 3.18)
project(ExynosTools LANGUAGES C)

# Permanent Xclipse 940 tuning flag for sources
add_compile_definitions(XCLIPSE_940_OPTIMIZE=1)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Staging pool default (1 MiB). Expose as cache so CI or users can override.
if(NOT DEFINED EXYNOSTOOLS_STAGING_POOL_SIZE)
  math(EXPR EXYNOSTOOLS_STAGING_POOL_SIZE "1 << 20")
  set(EXYNOSTOOLS_STAGING_POOL_SIZE ${EXYNOSTOOLS_STAGING_POOL_SIZE} CACHE STRING "Staging pool size bytes (Xclipse 940 tuned)")
endif()

# CMake module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Attempt to include helper modules if present; fail with clear message if missing
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/generate_shader_headers.cmake")
  include(cmake/generate_shader_headers.cmake)
else()
  message(FATAL_ERROR "Required CMake module missing: cmake/generate_shader_headers.cmake. Add it or restore from repo.")
endif()

# Optional helper that adds project include dirs; include if present, else add include dirs here.
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/add_project_includes.cmake")
  include(cmake/add_project_includes.cmake)
else()
  # Fallback: explicitly add common include dirs so sources can find headers like xeno_bc.h and generated shaders
  include_directories(
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/build_generated_spv"
  )
  message(STATUS "cmake/add_project_includes.cmake not found; using fallback include_directories")
endif()

# Find Vulkan (required)
find_package(Vulkan REQUIRED)

if(NOT Vulkan_FOUND)
  message(FATAL_ERROR "Vulkan not found. Install Vulkan SDK or libvulkan-dev.")
endif()

# Collect source files explicitly; skip missing files gracefully
set(EXYNOSTOOLS_SOURCES
  src/bc_emulate.c
  src/detect.c
  src/app_profile.c
  src/drivers/xclipse/async.c
  src/logging.c
)

set(EXYNOSTOOLS_EXISTING_SOURCES "")
foreach(f ${EXYNOSTOOLS_SOURCES})
  if(EXISTS "${CMAKE_SOURCE_DIR}/${f}")
    list(APPEND EXYNOSTOOLS_EXISTING_SOURCES ${f})
  else()
    message(STATUS "Skipping missing source: ${f}")
  endif()
endforeach()

# Define target after sources are resolved
add_executable(${PROJECT_NAME}
  ${EXYNOSTOOLS_EXISTING_SOURCES}
)

# Ensure gen_shaders target (created by generate_shader_headers.cmake) runs before build if it exists
if(TARGET gen_shaders)
  add_dependencies(${PROJECT_NAME} gen_shaders)
endif()

# Explicit target include directories (modern CMake)
target_include_directories(${PROJECT_NAME} PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/build_generated_spv"
)

# Link Vulkan
target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)

# Compiler flags tuned for performance on Xclipse 940; keep conservative portability
target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall -Wextra -Wno-unused-parameter
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(${PROJECT_NAME} PRIVATE -O3)
endif()

# Export staging pool size to C sources
target_compile_definitions(${PROJECT_NAME} PRIVATE
  EXYNOSTOOLS_STAGING_POOL_SIZE=${EXYNOSTOOLS_STAGING_POOL_SIZE}
)

# Optional microbenchmark target (if harness exists)
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/benchmark_xclipse.c")
  add_executable(exynostools_bench tools/benchmark_xclipse.c)
  target_link_libraries(exynostools_bench PUBLIC Vulkan::Vulkan)
  target_include_directories(exynostools_bench PUBLIC "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src")
  if(TARGET gen_shaders)
    add_dependencies(exynostools_bench gen_shaders)
  endif()
endif()

# Install (optional)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# Helpful status for CI and local dev (Xclipse 940 focus)
message(STATUS "ExynosTools configured (Xclipse 940 optimized). Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Generated headers dir: ${CMAKE_SOURCE_DIR}/include")
message(STATUS "Staging pool default size (bytes): ${EXYNOSTOOLS_STAGING_POOL_SIZE}")
