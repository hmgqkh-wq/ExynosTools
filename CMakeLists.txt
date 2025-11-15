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

# Source discovery
file(GLOB_RECURSE EXYNOSTOOLS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*/*.c"
)

if(NOT EXYNOSTOOLS_SOURCES)
  message(FATAL_ERROR "No source files found under src/ — check repository layout")
endif()

# Executable
add_executable(exynostools ${EXYNOSTOOLS_SOURCES})

# Include directories (project headers)
target_include_directories(exynostools
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Compiler options (common)
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

# Add AArch64-only linker flag (guarded)
if(TARGET_IS_AARCH64)
    message(STATUS "AArch64 target detected; applying AArch64-specific linker flags")
    # Only add the cortex fix flag on AArch64 builds
    target_link_options(exynostools PRIVATE "--fix-cortex-a53-843419")
else()
    message(STATUS "Non-AArch64 target detected; skipping AArch64-only linker flags")
endif()

# Vulkan: try to find and link if available
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "Vulkan found: ${Vulkan_LIBRARY}")
    target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
else()
    message(STATUS "Vulkan not found by CMake; build may still proceed but linking can fail if Vulkan symbols used")
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

# Install rules
install(TARGETS exynostools
    RUNTIME DESTINATION bin
)

# Helper target for CI debugging
add_custom_target(show_configuration
    COMMAND ${CMAKE_COMMAND} -E echo "CMake generator: ${CMAKE_GENERATOR}"
    COMMAND ${CMAKE_COMMAND} -E echo "Build type: ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "Target is aarch64: ${TARGET_IS_AARCH64}"
    COMMAND ${CMAKE_COMMAND} -E echo "Include dirs: ${CMAKE_CURRENT_SOURCE_DIR}/include;${CMAKE_CURRENT_SOURCE_DIR}/src"
    COMMAND ${CMAKE_COMMAND} -E echo "Source files found: ${EXYNOSTOOLS_SOURCES}"
)

# Developer notes (do not modify unless you understand toolchain)
# - For Android/NDK out-of-tree build example:
#     cmake -S . -B build-android -D CMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-31
#     cmake --build build-android -- -j$(nproc)
#
# - If CI injects extra linker flags, guard them here with TARGET_IS_AARCH64 check.
#
# - Ensure your CI config sets CMAKE_SYSTEM_NAME and CMAKE_SYSTEM_PROCESSOR correctly for cross builds.
