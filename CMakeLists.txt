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

# Include generator helper (fatal if missing)
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/generate_shader_headers.cmake")
  include(cmake/generate_shader_headers.cmake)
else()
  message(FATAL_ERROR "Required CMake module missing: cmake/generate_shader_headers.cmake.")
endif()

# Optional add_project_includes helper; fallback if missing
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/add_project_includes.cmake")
  include(cmake/add_project_includes.cmake)
else()
  include_directories(
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/build_generated_spv"
  )
  message(STATUS "cmake/add_project_includes.cmake not found; using fallback include_directories")
endif()

# Find Vulkan
find_package(Vulkan REQUIRED)

# Source list (only existing files included)
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

add_executable(${PROJECT_NAME}
  ${EXYNOSTOOLS_EXISTING_SOURCES}
)

# Ensure shader generation runs before build (if gen_shaders exists)
if(TARGET gen_shaders)
  add_dependencies(${PROJECT_NAME} gen_shaders)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/build_generated_spv"
)

target_link_libraries(${PROJECT_NAME} PUBLIC Vulkan::Vulkan)

target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall -Wextra -Wno-unused-parameter
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(${PROJECT_NAME} PRIVATE -O3)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
  EXYNOSTOOLS_STAGING_POOL_SIZE=${EXYNOSTOOLS_STAGING_POOL_SIZE}
)

# Optional benchmark target
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/benchmark_xclipse.c")
  add_executable(exynostools_bench tools/benchmark_xclipse.c)
  target_link_libraries(exynostools_bench PUBLIC Vulkan::Vulkan)
  target_include_directories(exynostools_bench PUBLIC "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/src")
  if(TARGET gen_shaders)
    add_dependencies(exynostools_bench gen_shaders)
  endif()
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

message(STATUS "ExynosTools configured (Xclipse 940 optimized). Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Generated headers dir: ${CMAKE_SOURCE_DIR}/include")
message(STATUS "Staging pool default size (bytes): ${EXYNOSTOOLS_STAGING_POOL_SIZE}")
