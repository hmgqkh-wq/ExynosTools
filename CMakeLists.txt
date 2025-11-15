cmake_minimum_required(VERSION 3.13)
project(ExynosTools C)

# Language and standard
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build type default
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Option to treat warnings as errors (useful in CI)
option(FAIL_ON_WARNINGS "Treat compiler warnings as errors" OFF)

# Source discovery (explicitly include top-level src tree)
file(GLOB_RECURSE EXYNOSTOOLS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*/*/*.c"
)

# Ensure you captured at least one source (defensive)
if(NOT EXYNOSTOOLS_SOURCES)
  message(FATAL_ERROR "No source files found under src/ — check repository layout")
endif()

# Executable target
add_executable(exynostools ${EXYNOSTOOLS_SOURCES})

# Public include path(s)
target_include_directories(exynostools
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"        # in-repo headers colocated with sources
)

# Compiler warnings and sanitizers
target_compile_options(exynostools PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wmissing-prototypes
)
if(FAIL_ON_WARNINGS)
    target_compile_options(exynostools PRIVATE -Werror)
endif()

# Platform and toolchain specific adjustments
# Detect aarch64 target to guard architecture-specific linker flags
set(TARGET_IS_AARCH64 FALSE)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_C_COMPILER_TARGET MATCHES "aarch64")
    set(TARGET_IS_AARCH64 TRUE)
endif()

# Add architecture-specific link option only when truly AArch64
if(TARGET_IS_AARCH64)
    message(STATUS "AArch64 target detected; applying AArch64-specific linker flags")
    # This is the intended flag; keep it only for AArch64
    target_link_options(exynostools PRIVATE "--fix-cortex-a53-843419")
else()
    message(STATUS "Non-AArch64 target detected; skipping AArch64-only linker flags")
endif()

# Try to find Vulkan and add include/link libraries if available
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    message(STATUS "Vulkan found: ${Vulkan_LIBRARY}")
    target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
else()
    message(STATUS "Vulkan not found by CMake; build will proceed but linking may fail if Vulkan symbols are used")
endif()

# Common defines usable across the project
target_compile_definitions(exynostools PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

# Optional: link with threads on platforms that need it
find_package(Threads QUIET)
if(THREADS_FOUND)
    target_link_libraries(exynostools PRIVATE Threads::Threads)
endif()

# Install (optional)
install(TARGETS exynostools
    RUNTIME DESTINATION bin
)

# Helpful CI debug target: prints found sources and include dirs
add_custom_target(show_configuration
    COMMAND ${CMAKE_COMMAND} -E echo "CMake generator: ${CMAKE_GENERATOR}"
    COMMAND ${CMAKE_COMMAND} -E echo "Build type: ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "Target is aarch64: ${TARGET_IS_AARCH64}"
    COMMAND ${CMAKE_COMMAND} -E echo "Include dirs: ${CMAKE_CURRENT_SOURCE_DIR}/include;${CMAKE_CURRENT_SOURCE_DIR}/src"
    COMMAND ${CMAKE_COMMAND} -E echo "Source count: $<TARGET_PROPERTY:exynostools,SOURCES>"
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/show_configuration.stamp
)

# Developer notes for CI (kept in-file for convenience)
# - Ensure Android NDK toolchain sets CMAKE_SYSTEM_NAME=Android and CMAKE_SYSTEM_PROCESSOR correctly.
# - If CI injects extra linker flags, guard them in this file (see AArch64 guard above).
# - If using an out-of-tree build, run:
#     cmake -S . -B build-android -D CMAKE_TOOLCHAIN_FILE=/path/to/ndk/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-31
#   then:
#     cmake --build build-android -- -j$(nproc)
