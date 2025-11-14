# cmake/add_project_includes.cmake
# SPDX-License-Identifier: MIT
if(NOT DEFINED PROJECT_INCLUDE_ADDED)
  set(PROJECT_INCLUDE_ADDED TRUE CACHE INTERNAL "Project include dirs added")
else()
  return()
endif()

set(PROJECT_SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(PROJECT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(GENERATED_SHADERS_DIR "${CMAKE_SOURCE_DIR}/include")
if(EXISTS "${CMAKE_SOURCE_DIR}/build_generated_spv")
  list(APPEND GENERATED_SHADERS_DIR "${CMAKE_SOURCE_DIR}/build_generated_spv")
endif()

include_directories(
  "${PROJECT_INCLUDE_DIR}"
  "${PROJECT_SRC_DIR}"
  "${GENERATED_SHADERS_DIR}"
)

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
