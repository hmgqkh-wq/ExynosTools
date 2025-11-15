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

# Root paths (adjust if your repo layout changes)
set(PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SRC_DIR "${PROJ_ROOT}/src")
set(INCLUDE_DIR "${PROJ_ROOT}/include")

# Discover all .c sources under src
file(GLOB_RECURSE ALL_C_SOURCES
    "${SRC_DIR}/*.c"
)

if(NOT ALL_C_SOURCES)
  message(FATAL_ERROR "No C source files found under ${SRC_DIR} — check repository layout")
endif()

# Identify the dedicated logging implementation source (the TU that must provide non-inline linkage).
# If your logging TU is at src/logging.c, we treat it specially by compiling it as an OBJECT target
# with XENO_LOG_IMPLEMENTATION_PRESENT defined for that TU only.
set(LOGGING_REL_PATH "src/logging.c")
set(LOGGING_ABS_PATH "${PROJ_ROOT}/${LOGGING_REL_PATH}")

# Ensure logging file exists before applying special handling
set(HAVE_LOGGING_SRC OFF)
foreach(f ${ALL_C_SOURCES})
  if(f MATCHES "${LOGGING_ABS_PATH}$")
    set(HAVE_LOGGING_SRC ON)
  endif()
endforeach()

# Build an OBJECT library for logging.c when present and define XENO_LOG_IMPLEMENTATION_PRESENT for it.
set(OTHER_SOURCES ${ALL_C_SOURCES})
if(HAVE_LOGGING_SRC)
  list(REMOVE_ITEM OTHER_SOURCES "${LOGGING_ABS_PATH}")
  add_library(logging_obj OBJECT "${LOGGING_ABS_PATH}")
  target_include_directories(logging_obj PRIVATE "${INCLUDE_DIR}" "${SRC_DIR}")
  target_compile_definitions(logging_obj PRIVATE XENO_LOG_IMPLEMENTATION_PRESENT)
  target_compile_options(logging_obj PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wmissing-prototypes)
endif()

# Create the main executable from the remaining sources and the logging object (if created)
add_executable(exynostools ${OTHER_SOURCES})

# If logging_obj exists, link its objects into the executable via $<TARGET_OBJECTS:...>
if(HAVE_LOGGING_SRC)
  target_sources(exynostools PRIVATE $<TARGET_OBJECTS:logging_obj>)
endif()

# Ensure include/ and src/ are provided to all translation units
target_include_directories(exynostools
    PRIVATE
        "${INCLUDE_DIR}"
        "${SRC_DIR}"
)

# Compiler flags for the main target
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

# Useful preprocessor defines (Debug flag to TUs when building Debug)
target_compile_definitions(exynostools PRIVATE
    $<$<CONFIG:Debug>:DEBUG=1>
)

# Install rules
install(TARGETS exynostools
    RUNTIME DESTINATION bin
)

# Helper target for CI debugging: prints configuration and discovered sources
add_custom_target(show_configuration
    COMMAND ${CMAKE_COMMAND} -E echo "CMake generator: ${CMAKE_GENERATOR}"
    COMMAND ${CMAKE_COMMAND} -E echo "Build type: ${CMAKE_BUILD_TYPE}"
    COMMAND ${CMAKE_COMMAND} -E echo "Have logging TU (src/logging.c): ${HAVE_LOGGING_SRC}"
    COMMAND ${CMAKE_COMMAND} -E echo "Target is aarch64: ${TARGET_IS_AARCH64}"
    COMMAND ${CMAKE_COMMAND} -E echo "Include dirs: ${INCLUDE_DIR};${SRC_DIR}"
    COMMAND ${CMAKE_COMMAND} -E echo "Source count (all): ${ALL_C_SOURCES}"
)

# Developer notes
# - The build arranges so that src/logging.c is compiled with XENO_LOG_IMPLEMENTATION_PRESENT
#   thus include/xeno_log.h exposes non-inline prototypes only for that TU, avoiding static vs non-static conflicts.
# - Other TUs include include/xeno_log.h and will either get the inline fallbacks (if logging.c is not present)
#   or will link to the single logging implementation (if logging.c is present).
# - To rebuild cleanly in CI, remove the previous build directory entirely between attempts:
#     rm -rf build-android && cmake -S . -B build-android <toolchain args> && cmake --build build-android -- -j$(nproc)
#
# - For Android/NDK out-of-tree build example:
#     cmake -S . -B build-android -D CMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-31
#     cmake --build build-android -- -j$(nproc)
