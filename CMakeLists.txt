cmake_minimum_required(VERSION 3.13)
project(ExynosTools C)

# Basic toolchain/ABI detection (Android/Ninja/CMake)
if (NOT DEFINED CMAKE_SYSTEM_NAME)
    set(CMAKE_SYSTEM_NAME "Android" CACHE STRING "" FORCE)
endif()

# Example target properties — adjust your actual sources and targets as needed.
file(GLOB_RECURSE EXYNOSTOOLS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.c"
)

add_executable(exynostools ${EXYNOSTOOLS_SOURCES})

# Standard compile options
target_compile_features(exynostools PRIVATE c_std_99)
target_compile_options(exynostools PRIVATE -Wall -Wextra -Wno-unused-parameter)

# Linker/arch-specific flags: only add cortex fix for AArch64 targets.
if(CMAKE_C_COMPILER_TARGET MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    message(STATUS "Configuring AArch64-specific linker flags")
    # Use correct form for clang/ld; do not add stray leading dot. Example only:
    target_link_options(exynostools PRIVATE "--fix-cortex-a53-843419")
else()
    message(STATUS "Non-AArch64 target detected; skipping cortex fix linker flag")
endif()

# Link with Vulkan if available
find_package(Vulkan REQUIRED)
if (Vulkan_FOUND)
    target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
endif()

# Install rules (optional)
install(TARGETS exynostools DESTINATION bin)
