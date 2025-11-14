cmake_minimum_required(VERSION 3.16)
project(ExynosTools LANGUAGES C CXX)

# Build type defaults
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Useful compiler flags (adjust as needed)
if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_C_COMPILER_ID MATCHES "GNU")
  add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Make sure we have an out-of-source build
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(WARNING "In-source builds are discouraged. Consider using a build/ directory.")
endif()

# ------------------------------------------------------------------
# Shader generation
# Include your generator early so headers are available before targets
# This file is OPTIONAL to avoid hard error if it isn't present locally
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/generate_shader_headers.cmake")
  include("${CMAKE_SOURCE_DIR}/cmake/generate_shader_headers.cmake")
else()
  message(STATUS "cmake/generate_shader_headers.cmake not found; continuing (CI or manual generation expected).")
endif()

# Ensure generated headers in source include/ or build tree are found by compiler
# Prefer source include/ (so committed headers are used if present).
include_directories(
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_BINARY_DIR}/generated_shaders"
)

# Provide a CACHE option so CI or users can disable generator if desired
option(GENERATE_SHADERS "Run SPIR-V shader generation step" ON)
if(GENERATE_SHADERS)
  # If the generate_shader_headers.cmake defined a gen_shaders target, it was created above.
  # If not, add a placeholder target so dependent targets can still add_dependencies safely.
  if(NOT TARGET gen_shaders)
    add_custom_target(gen_shaders)
  endif()
endif()

# ------------------------------------------------------------------
# Project layout detection
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")

# Collect source files if no src/CMakeLists.txt is present
if(EXISTS "${SRC_DIR}/CMakeLists.txt")
  add_subdirectory("${SRC_DIR}")
else()
  # Basic heuristic: gather .c and .cpp files under src/
  file(GLOB_RECURSE EXYNOSTOOLS_SRCS "${SRC_DIR}/*.c" "${SRC_DIR}/*.cpp" "${SRC_DIR}/*.cc")
  if(NOT EXYNOSTOOLS_SRCS)
    message(WARNING "No source files found under ${SRC_DIR}. Add a subdirectory or populate src/ with sources.")
  endif()

  # Create the target (adjust name and properties as your project expects)
  add_library(exynostools STATIC ${EXYNOSTOOLS_SRCS})

  # Ensure shader generation happens before building the library
  add_dependencies(exynostools gen_shaders)

  # Public include dirs
  target_include_directories(exynostools PUBLIC
    "${INCLUDE_DIR}"
    "${CMAKE_BINARY_DIR}/generated_shaders"
  )

  # Example compile options and definitions for Android cross builds; keep harmless if unused
  if(ANDROID)
    target_compile_definitions(exynostools PRIVATE ANDROID)
  endif()

  # Example: set C standard if needed
  set_target_properties(exynostools PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
  )
endif()

# ------------------------------------------------------------------
# Install rules (optional, adjust as needed)
install(DIRECTORY "${INCLUDE_DIR}/" DESTINATION include OPTIONAL)

# Export useful status
message(STATUS "CMake binary dir: ${CMAKE_BINARY_DIR}")
message(STATUS "Include directories: ${INCLUDE_DIR}, ${CMAKE_BINARY_DIR}/generated_shaders")
message(STATUS "Shader generation: ${GENERATE_SHADERS}")
