cmake_minimum_required(VERSION 3.13)
project(ExynosTools C)

# Language and standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Options
option(FAIL_ON_WARNINGS "Treat compiler warnings as errors" OFF)
option(ENABLE_VERBOSE_BUILD "Show verbose compiler/linker commands" OFF)

# Project paths
set(PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SRC_DIR "${PROJ_ROOT}/src")
set(INCLUDE_DIR "${PROJ_ROOT}/include")

# Gather only .c sources (relative then convert to absolute), skip common asset/shader folders
file(GLOB_RECURSE ALL_C_SOURCES_RAW
    RELATIVE "${PROJ_ROOT}"
    "${SRC_DIR}/*.c"
)

set(ALL_C_SOURCES "")
foreach(f ${ALL_C_SOURCES_RAW})
  string(FIND "${f}" "/shaders/" _has_shaders_dir)
  string(FIND "${f}" "/shader/" _has_shader_dir)
  string(FIND "${f}" "/assets/" _has_assets_dir)
  string(FIND "${f}" "/third_party/" _has_thirdparty_dir)
  if(_has_shaders_dir EQUAL -1 AND _has_shader_dir EQUAL -1 AND _has_assets_dir EQUAL -1 AND _has_thirdparty_dir EQUAL -1)
    list(APPEND ALL_C_SOURCES "${PROJ_ROOT}/${f}")
  endif()
endforeach()

if(NOT ALL_C_SOURCES)
  message(FATAL_ERROR "No C source files found under ${SRC_DIR} — check repository layout")
endif()

# Special-case: compile src/logging.c as OBJECT with XENO_LOG_IMPLEMENTATION_PRESENT if present
set(LOGGING_REL_PATH "src/logging.c")
set(LOGGING_ABS_PATH "${PROJ_ROOT}/${LOGGING_REL_PATH}")

set(HAVE_LOGGING_SRC OFF)
foreach(f ${ALL_C_SOURCES})
  if(f STREQUAL "${LOGGING_ABS_PATH}")
    set(HAVE_LOGGING_SRC ON)
  endif()
endforeach()

set(MAIN_SOURCES ${ALL_C_SOURCES})
if(HAVE_LOGGING_SRC)
  list(REMOVE_ITEM MAIN_SOURCES "${LOGGING_ABS_PATH}")
  add_library(logging_obj OBJECT "${LOGGING_ABS_PATH}")
  target_include_directories(logging_obj PRIVATE "${INCLUDE_DIR}" "${SRC_DIR}")
  target_compile_definitions(logging_obj PRIVATE XENO_LOG_IMPLEMENTATION_PRESENT)
  target_compile_options(logging_obj PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# Main executable
add_executable(exynostools ${MAIN_SOURCES})

# Attach logging object files if present
if(HAVE_LOGGING_SRC)
  target_sources(exynostools PRIVATE $<TARGET_OBJECTS:logging_obj>)
endif()

# Ensure headers are visible to all translation units
target_include_directories(exynostools
    PRIVATE
        "${INCLUDE_DIR}"
        "${SRC_DIR}"
)

# Compiler options: keep warnings but avoid treating missing-prototypes as fatal here
target_compile_options(exynostools PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
)
if(FAIL_ON_WARNINGS)
    target_compile_options(exynostools PRIVATE -Werror)
endif()
if(ENABLE_VERBOSE_BUILD)
    set(CMAKE_VERBOSE_MAKEFILE ON)
endif()

# Platform / architecture detection
set(TARGET_IS_AARCH64 FALSE)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_C_COMPILER_TARGET MATCHES "aarch64")
    set(TARGET_IS_AARCH64 TRUE)
endif()

if(TARGET_IS_AARCH64)
    message(STATUS "AArch64 target detected; applying AArch64-specific linker flags")
    target_link_options(exynostools PRIVATE "--fix-cortex-a53-843419")
else()
    message(STATUS "Non-AArch64 target detected; skipping AArch64-only linker flags")
endif()

# Vulkan optional linking
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
else()
    message(STATUS "Vulkan not found by CMake; linking may fail if Vulkan symbols used")
endif()

# Threads
find_package(Threads QUIET)
if(THREADS_FOUND)
    target_link_libraries(exynostools PRIVATE Threads::Threads)
endif()

# Debug define
target_compile_definitions(exynostools PRIVATE
    $<$<CONFIG:Debug>:DEBUG=1>
)

# Install rules
install(TARGETS exynostools
    RUNTIME DESTINATION bin
)

# Safe debug helper: join the list into a single string to avoid executing file paths
string(REPLACE ";" " " ALL_C_SOURCES_JOINED "${ALL_C_SOURCES}")

add_custom_target(show_sources
  COMMENT "Printing discovered C sources (safe)"
  COMMAND ${CMAKE_COMMAND} -E echo "Discovered C sources:"
  COMMAND ${CMAKE_COMMAND} -E echo "${ALL_C_SOURCES_JOINED}"
)

# Small helper target to show configuration info
add_custom_target(show_configuration
  COMMENT "Project configuration summary"
  COMMAND ${CMAKE_COMMAND} -E echo "CMake generator: ${CMAKE_GENERATOR}"
  COMMAND ${CMAKE_COMMAND} -E echo "Build type: ${CMAKE_BUILD_TYPE}"
  COMMAND ${CMAKE_COMMAND} -E echo "Have logging TU (src/logging.c): ${HAVE_LOGGING_SRC}"
  COMMAND ${CMAKE_COMMAND} -E echo "Target is aarch64: ${TARGET_IS_AARCH64}"
  COMMAND ${CMAKE_COMMAND} -E echo "Include dirs: ${INCLUDE_DIR};${SRC_DIR}"
)

message(STATUS "ExynosTools configured. Source count: ${ALL_C_SOURCES_JOINED}")
