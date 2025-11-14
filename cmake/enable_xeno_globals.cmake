# SPDX-License-Identifier: MIT
# cmake/enable_xeno_globals.cmake
# Usage: include this file from your top-level CMakeLists or target file.
# It marks src/xeno_wrapper.c as the single provider of the Vulkan globals.

# Adjust the relative path if your sources live elsewhere.
if (EXISTS "${CMAKE_SOURCE_DIR}/src/xeno_wrapper.c")
    set_source_files_properties("${CMAKE_SOURCE_DIR}/src/xeno_wrapper.c"
        PROPERTIES COMPILE_DEFINITIONS "PROVIDE_VK_GLOBALS")
else()
    message(WARNING "xeno_wrapper.c not found at ${CMAKE_SOURCE_DIR}/src/xeno_wrapper.c; PROVIDE_VK_GLOBALS not set")
endif()
