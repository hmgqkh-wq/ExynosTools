# cmake/add_project_includes.cmake
# SPDX-License-Identifier: MIT
# Ensures project and generated include directories are visible to all targets.
# Safe to include multiple times.

if(NOT DEFINED PROJECT_INCLUDE_ADDED)
  set(PROJECT_INCLUDE_ADDED TRUE CACHE INTERNAL "Project include dirs added")
else()
  return()
endif()

# Source and generated include dirs
set(PROJECT_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(PROJECT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(GENERATED_SHADERS_DIR "${CMAKE_SOURCE_DIR}/include") # emitter writes headers into include/
# (fallback if your generator writes elsewhere)
if(EXISTS "${CMAKE_SOURCE_DIR}/build_generated_spv")
  list(APPEND GENERATED_SHADERS_DIR "${CMAKE_SOURCE_DIR}/build_generated_spv")
endif()

# Add include directories globally so builds invoked before target registration still see headers
include_directories(
  "${PROJECT_INCLUDE_DIR}"
  "${PROJECT_SRC_DIR}"
  "${GENERATED_SHADERS_DIR}"
)

# Additionally export via usage requirements for targets created later
# Provide a function to attach include dirs to specific targets (safe no-op if target missing)
function(project_propagate_includes target)
  if(TARGET "${target}")
    target_include_directories("${target}" PUBLIC
      "${PROJECT_INCLUDE_DIR}"
      "${PROJECT_SRC_DIR}"
      "${GENERATED_SHADERS_DIR}"
    )
  endif()
endfunction()

message(STATUS "Project include dirs: ${PROJECT_INCLUDE_DIR}; src: ${PROJECT_SRC_DIR}; generated: ${GENERATED_SHADERS_DIR}")
