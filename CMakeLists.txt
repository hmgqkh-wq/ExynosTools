cmake_minimum_required(VERSION 3.13)
project(ExynosTools C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(FAIL_ON_WARNINGS "Treat compiler warnings as errors" OFF)
option(ENABLE_VERBOSE_BUILD "Show verbose compiler/linker commands" OFF)

set(PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SRC_DIR "${PROJ_ROOT}/src")
set(INCLUDE_DIR "${PROJ_ROOT}/include")

# Gather only .c sources. Exclude known non-C directories (shaders, assets).
file(GLOB_RECURSE ALL_C_SOURCES_RAW
    RELATIVE "${PROJ_ROOT}"
    "${SRC_DIR}/*.c"
)

# Filter out shader/asset dirs in case they lie under src/
set(ALL_C_SOURCES "")
foreach(f ${ALL_C_SOURCES_RAW})
  string(FIND "${f}" "shaders/" _has_shaders_dir)
  string(FIND "${f}" "shader/" _has_shader_dir)
  string(FIND "${f}" "assets/" _has_assets_dir)
  if(_has_shaders_dir EQUAL -1 AND _has_shader_dir EQUAL -1 AND _has_assets_dir EQUAL -1)
    list(APPEND ALL_C_SOURCES "${PROJ_ROOT}/${f}")
  endif()
endforeach()

if(NOT ALL_C_SOURCES)
  message(FATAL_ERROR "No C source files found under ${SRC_DIR} — check repository layout")
endif()

# Special-case logging TU: compile src/logging.c with XENO_LOG_IMPLEMENTATION_PRESENT
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

add_executable(exynostools ${MAIN_SOURCES})

if(HAVE_LOGGING_SRC)
  target_sources(exynostools PRIVATE $<TARGET_OBJECTS:logging_obj>)
endif()

# Ensure headers visible
target_include_directories(exynostools
    PRIVATE
        "${INCLUDE_DIR}"
        "${SRC_DIR}"
)

# Compiler flags (do not add -Wmissing-prototypes here)
target_compile_options(exynostools PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
)
if(FAIL_ON_WARNINGS)
    target_compile_options(exynostools PRIVATE -Werror)
endif()
if(ENABLE_VERBOSE_BUILD)
    set(CMAKE_VERBOSE_MAKEFILE ON)
endif()

# Arch detection
set(TARGET_IS_AARCH64 FALSE)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_C_COMPILER_TARGET MATCHES "aarch64")
    set(TARGET_IS_AARCH64 TRUE)
endif()

if(TARGET_IS_AARCH64)
    target_link_options(exynostools PRIVATE "--fix-cortex-a53-843419")
endif()

# Vulkan (optional)
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
endif()

find_package(Threads QUIET)
if(THREADS_FOUND)
    target_link_libraries(exynostools PRIVATE Threads::Threads)
endif()

target_compile_definitions(exynostools PRIVATE
    $<$<CONFIG:Debug>:DEBUG=1>
)

install(TARGETS exynostools RUNTIME DESTINATION bin)

# Debugging helper: list discovered C sources
add_custom_target(show_sources ALL
  COMMAND ${CMAKE_COMMAND} -E echo "Discovered C sources:"
  COMMAND ${CMAKE_COMMAND} -E echo "${ALL_C_SOURCES}"
)
