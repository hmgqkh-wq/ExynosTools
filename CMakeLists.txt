cmake_minimum_required(VERSION 3.16)
project(ExynosTools C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Paths
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(GENERATED_SPV_DIR "${CMAKE_BINARY_DIR}/generated_spv")

# Include directories (generated headers are placed into GENERATED_SPV_DIR)
include_directories(
  "${INCLUDE_DIR}"
  "${SRC_DIR}"
  "${GENERATED_SPV_DIR}"
)

# Compiler options
add_compile_options(-Wall -Wextra -Wno-unused-parameter $<$<CONFIG:Release>:-O3>)

# Source files (core minimal set; additional files may be added)
file(GLOB_RECURSE PROJECT_SOURCES
  "${SRC_DIR}/*.c"
)

add_executable(exynostools ${PROJECT_SOURCES})

target_compile_definitions(exynostools PRIVATE
  EXYNOSTOOLS_STAGING_POOL_SIZE=1048576
  XCLIPSE_940_OPTIMIZE=1
)

# Link to Vulkan if available
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(VULKAN_PKG QUIET vulkan)
  if(VULKAN_PKG_FOUND)
    target_include_directories(exynostools PRIVATE ${VULKAN_PKG_INCLUDE_DIRS})
    target_link_libraries(exynostools PRIVATE ${VULKAN_PKG_LDFLAGS})
  else()
    find_library(VULKAN_LIB NAMES vulkan Vulkan)
    if(VULKAN_LIB)
      target_link_libraries(exynostools PRIVATE ${VULKAN_LIB})
    endif()
  endif()
else()
  find_library(VULKAN_LIB NAMES vulkan Vulkan)
  if(VULKAN_LIB)
    target_link_libraries(exynostools PRIVATE ${VULKAN_LIB})
  endif()
endif()

find_package(Threads)
if(THREADS_FOUND)
  target_link_libraries(exynostools PRIVATE Threads::Threads)
endif()

# Ensure generated SPV headers are considered a dependency for build
add_custom_target(gen_shaders ALL
  COMMAND ${CMAKE_COMMAND} -E echo "Place generated SPIR-V headers into ${GENERATED_SPV_DIR}"
)

add_dependencies(exynostools gen_shaders)

message(STATUS "Configured ExynosTools project")
