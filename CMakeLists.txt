cmake_minimum_required(VERSION 3.16)
project(ExynosTools LANGUAGES C CXX)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Include generator early
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/generate_shader_headers.cmake")
  include("${CMAKE_SOURCE_DIR}/cmake/generate_shader_headers.cmake")
else()
  message(STATUS "Shader generator cmake file not found; CI script will generate headers.")
endif()

# Include dirs (source include/ and build-generated, if used)
include_directories("${CMAKE_SOURCE_DIR}/include" "${CMAKE_BINARY_DIR}/generated_shaders")

# Collect sources
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
file(GLOB_RECURSE EXYNOSTOOLS_SRCS "${SRC_DIR}/*.c" "${SRC_DIR}/*.cpp")

# Library target (adjust if you use a different target name)
add_library(exynostools STATIC ${EXYNOSTOOLS_SRCS})

# Ensure shader headers are ready before compile
if(TARGET gen_shaders)
  add_dependencies(exynostools gen_shaders)
endif()

target_include_directories(exynostools PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_BINARY_DIR}/generated_shaders"
)

set_target_properties(exynostools PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
)

# Optional warnings
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(exynostools PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Install headers (optional)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/" DESTINATION include OPTIONAL)

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Include dirs: ${CMAKE_SOURCE_DIR}/include ; ${CMAKE_BINARY_DIR}/generated_shaders")
