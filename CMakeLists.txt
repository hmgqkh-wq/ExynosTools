# SPDX-License-Identifier: MIT
# CMakeLists.txt - build ExynosTools as a shared library (ready to copy)
cmake_minimum_required(VERSION 3.15)
project(ExynosTools C)

# Basic build settings
set(CMAKE_C_STANDARD 99)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

# Source collection: include all .c under src/ (adjust if you have special exclusions)
file(GLOB_RECURSE EXYNOSTOOLS_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.c"
)

# Public include directories
include_directories(
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/build-android/shaders/src"
)

# Ensure cmake/enable_xeno_globals.cmake runs (it marks xeno_wrapper.c to export globals)
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/enable_xeno_globals.cmake")
    include("${CMAKE_SOURCE_DIR}/cmake/enable_xeno_globals.cmake")
endif()

# Create the shared library target
add_library(exynostools SHARED ${EXYNOSTOOLS_SOURCES})

# Compile definitions and platform-specific flags
target_compile_definitions(exynostools PRIVATE ANDROID)
# Keep format-security warnings as in CI; change if necessary:
target_compile_options(exynostools PRIVATE -Wformat -Werror=format-security)

# Link against typical Android libs (adjust if your project needs more)
find_library(LOG_LIB log)
find_library(ANDROID_LIB android)

if(LOG_LIB)
    target_link_libraries(exynostools PRIVATE ${LOG_LIB})
endif()
if(ANDROID_LIB)
    target_link_libraries(exynostools PRIVATE ${ANDROID_LIB})
endif()

# Install rules (optional): place the produced .so into package_temp/usr/lib
install(TARGETS exynostools
    LIBRARY DESTINATION package_temp/usr/lib
)

# Export compile commands for tooling (optional)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
