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

# Root paths
set(PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SRC_DIR "${PROJ_ROOT}/src")
set(INCLUDE_DIR "${PROJ_ROOT}/include")

# Gather sources
file(GLOB_RECURSE ALL_C_SOURCES
    "${SRC_DIR}/*.c"
)

if(NOT ALL_C_SOURCES)
  message(FATAL_ERROR "No C source files found under ${SRC_DIR} — check repository layout")
endif()

# Special-case logging TU: compile src/logging.c with XENO_LOG_IMPLEMENTATION_PRESENT
set(LOGGING_REL_PATH "src/logging.c")
set(LOGGING_ABS_PATH "${PROJ_ROOT}/${LOGGING_REL_PATH}")

set(HAVE_LOGGING_SRC OFF)
foreach(f ${ALL_C_SOURCES})
  if(f MATCHES "${LOGGING_ABS_PATH}$")
    set(HAVE_LOGGING_SRC ON)
  endif()
endforeach()

# Remove logging.c from main source list if present; build as OBJECT with define
set(MAIN_SOURCES ${ALL_C_SOURCES})
if(HAVE_LOGGING_SRC)
  list(REMOVE_ITEM MAIN_SOURCES "${LOGGING_ABS_PATH}")
  add_library(logging_obj OBJECT "${LOGGING_ABS_PATH}")
  target_include_directories(logging_obj PRIVATE "${INCLUDE_DIR}" "${SRC_DIR}")
  target_compile_definitions(logging_obj PRIVATE XENO_LOG_IMPLEMENTATION_PRESENT)
  target_compile_options(logging_obj PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wmissing-prototypes)
endif()

# Create executable from remaining sources
add_executable(exynostools ${MAIN_SOURCES})

# Link objects from logging_obj if created
if(HAVE_LOGGING_SRC)
  target_sources(exynostools PRIVATE $<TARGET_OBJECTS:logging_obj>)
endif()

# Include directories
target_include_directories(exynostools
    PRIVATE
        "${INCLUDE_DIR}"
        "${SRC_DIR}"
)

# Compiler options
target_compile_options(exynostools PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wmissing-prototypes
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

# AArch64-only linker flag (guarded)
if(TARGET_IS_AARCH64)
    message(STATUS "AArch64 target detected; applying AArch64-specific linker flags")
    target_link_options(exynostools PRIVATE "--fix-cortex-a53-843419")
else()
    message(STATUS "Non-AArch64 target detected; skipping AArch64-only linker flags")
endif()

# Vulkan: optional
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "Vulkan found: ${Vulkan_LIBRARY}")
    target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
else()
    message(STATUS "Vulkan not found by CMake; linking may fail if Vulkan symbols are used at link time")
endif()

# Threads
find_package(Threads QUIET)
if(THREADS_FOUND)
    target_link_libraries(exynostools PRIVATE Threads::Threads)
endif()

# Useful preprocessor defines
target_compile_definitions(exynostools PRIVATE
    $<$<CONFIG:Debug>:DEBUG=1>
)

# Install rule
install(TARGETS exynostools
    RUNTIME DESTINATION bin
)

# CI/developer helper targets
add_custom_target(show_configuration
    COMMAND ${CMAKE_COMMAND} -E echo "CMake generator: ${CMAKE_GENERATOR}"
    COMMAND ${CMAKE_COMMAND} -E echo "Build type: ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "Have logging TU (src/logging.c): ${HAVE_LOGGING_SRC}"
    COMMAND ${CMAKE_COMMAND} -E echo "Target is aarch64: ${TARGET_IS_AARCH64}"
    COMMAND ${CMAKE_COMMAND} -E echo "Include dirs: ${INCLUDE_DIR};${SRC_DIR}"
)

add_custom_target(clean_build_dir
    COMMAND ${CMAKE_COMMAND} -E echo "Use external script or rm -rf <build-dir> to fully clean. This target only displays guidance."
)

# Helpful messages when configuring
message(STATUS "ExynosTools configuration complete")
message(STATUS "Sources: ${ALL_C_SOURCES}")
message(STATUS "Include dir: ${INCLUDE_DIR}")
