# SPDX-License-Identifier: MIT
# cmake/enable_xeno_globals.cmake
# Mark only src/xeno_wrapper.c as the single provider of Vulkan globals.
# Drop this file into cmake/ and include it from your top-level CMakeLists.txt.

set(XENO_WRAPPER_SRC "${CMAKE_SOURCE_DIR}/src/xeno_wrapper.c")

if(EXISTS "${XENO_WRAPPER_SRC}")
    message(STATUS "Enabling PROVIDE_VK_GLOBALS for ${XENO_WRAPPER_SRC}")
    set_source_files_properties("${XENO_WRAPPER_SRC}"
        PROPERTIES COMPILE_DEFINITIONS "PROVIDE_VK_GLOBALS")
else()
    message(WARNING "xeno_wrapper.c not found at ${XENO_WRAPPER_SRC}; PROVIDE_VK_GLOBALS not set")
endif()
