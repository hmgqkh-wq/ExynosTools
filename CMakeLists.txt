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

# Paths
set(PROJ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SRC_DIR "${PROJ_ROOT}/src")
set(INCLUDE_DIR "${PROJ_ROOT}/include")
set(SHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/shaders")

# -------------------------
# Shader discovery and compile-to-SPV target (full behavior)
# - Compiles .comp .frag .vert .glsl to .spv in build tree
# - Ensures shaders are NOT treated as C sources
# - If glslangValidator not found, configuration proceeds but compile_shaders target will fail at build
# -------------------------
file(GLOB_RECURSE SHADER_SOURCES_RAW
  RELATIVE "${PROJ_ROOT}"
  "${SRC_DIR}/*.comp"
  "${SRC_DIR}/*.frag"
  "${SRC_DIR}/*.vert"
  "${SRC_DIR}/*.glsl"
)

set(SPIRV_OUTPUTS "")
foreach(s ${SHADER_SOURCES_RAW})
  # absolute input path
  set(IN_ABS "${PROJ_ROOT}/${s}")
  # compute output path under build/shaders/same/dirs with .spv extension
  get_filename_component(DIR_PART "${s}" DIRECTORY)
  get_filename_component(NAME_WE "${s}" NAME_WE)
  set(OUT_ABS "${SHADER_BUILD_DIR}/${DIR_PART}/${NAME_WE}.spv")
  list(APPEND SPIRV_OUTPUTS "${OUT_ABS}")

  add_custom_command(
    OUTPUT "${OUT_ABS}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<SHELL_PATH:${SHADER_BUILD_DIR}/${DIR_PART}>"
    COMMAND ${CMAKE_COMMAND} -E echo "Compiling shader ${IN_ABS} -> ${OUT_ABS}"
    COMMAND glslangValidator -V "${IN_ABS}" -o "${OUT_ABS}"
    DEPENDS "${IN_ABS}"
    COMMENT "Compiling shader: ${s}"
    VERBATIM
  )
endforeach()

if(SPIRV_OUTPUTS)
  add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})
endif()

# -------------------------
# Gather C sources only (explicitly exclude shaders and common asset dirs)
# -------------------------
file(GLOB_RECURSE ALL_C_SOURCES_RAW
    RELATIVE "${PROJ_ROOT}"
    "${SRC_DIR}/*.c"
)

set(ALL_C_SOURCES "")
foreach(f ${ALL_C_SOURCES_RAW})
  string(FIND "${f}" ".comp" _a)
  string(FIND "${f}" ".frag" _b)
  string(FIND "${f}" ".vert" _c)
  string(FIND "${f}" ".glsl" _d)
  string(FIND "${f}" "/shaders/" _s)
  string(FIND "${f}" "/shader/" _ss)
  string(FIND "${f}" "/assets/" _a2)
  if(_a EQUAL -1 AND _b EQUAL -1 AND _c EQUAL -1 AND _d EQUAL -1 AND _s EQUAL -1 AND _ss EQUAL -1 AND _a2 EQUAL -1)
    list(APPEND ALL_C_SOURCES "${PROJ_ROOT}/${f}")
  endif()
endforeach()

if(NOT ALL_C_SOURCES)
  message(FATAL_ERROR "No C source files found under ${SRC_DIR} — check repository layout")
endif()

# -------------------------
# Logging TU special-case: compile src/logging.c as OBJECT with XENO_LOG_IMPLEMENTATION_PRESENT
# -------------------------
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
  target_compile_definitions(logging_obj PRIVATE XENO_LOG_IMPLEMENTATION_PRESENT=1)
  target_compile_options(logging_obj PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# -------------------------
# Main executable
# -------------------------
add_executable(exynostools ${MAIN_SOURCES})

if(HAVE_LOGGING_SRC)
  target_sources(exynostools PRIVATE $<TARGET_OBJECTS:logging_obj>)
endif()

# Include dirs for all TUs
target_include_directories(exynostools
    PRIVATE
        "${INCLUDE_DIR}"
        "${SRC_DIR}"
)

# Compiler flags: keep warnings but avoid forcing missing-prototypes fatal here
target_compile_options(exynostools PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
)

if(FAIL_ON_WARNINGS)
    target_compile_options(exynostools PRIVATE -Werror)
endif()
if(ENABLE_VERBOSE_BUILD)
    set(CMAKE_VERBOSE_MAKEFILE ON)
endif()

# -------------------------
# Platform / arch detection and selective linker flags
# -------------------------
set(TARGET_IS_AARCH64 FALSE)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_C_COMPILER_TARGET MATCHES "aarch64")
    set(TARGET_IS_AARCH64 TRUE)
endif()

if(TARGET_IS_AARCH64)
    message(STATUS "AArch64 target detected; applying AArch64-specific linker flags if supported")
    # Guarded addition: append only when target supports it. If unsupported by toolchain,
    # clang will error; this is intentionally conservative — remove if your toolchain doesn't accept it.
    target_link_options(exynostools PRIVATE "--fix-cortex-a53-843419")
endif()

# Vulkan optional
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
else()
    message(STATUS "Vulkan not found by CMake; link may fail if Vulkan symbols used")
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

# Install
install(TARGETS exynostools RUNTIME DESTINATION bin)

# -------------------------
# Safe debug helpers (print lists as single arguments to avoid execution)
# -------------------------
string(REPLACE ";" " " ALL_C_SOURCES_JOINED "${ALL_C_SOURCES}")
add_custom_target(show_sources
  COMMENT "Printing discovered C sources (safe)"
  COMMAND ${CMAKE_COMMAND} -E echo "Discovered C sources:"
  COMMAND ${CMAKE_COMMAND} -E echo "${ALL_C_SOURCES_JOINED}"
)

add_custom_target(show_configuration
  COMMENT "Project configuration summary"
  COMMAND ${CMAKE_COMMAND} -E echo "CMake generator: ${CMAKE_GENERATOR}"
  COMMAND ${CMAKE_COMMAND} -E echo "Build type: ${CMAKE_BUILD_TYPE}"
  COMMAND ${CMAKE_COMMAND} -E echo "Have logging TU (src/logging.c): ${HAVE_LOGGING_SRC}"
  COMMAND ${CMAKE_COMMAND} -E echo "Target is aarch64: ${TARGET_IS_AARCH64}"
  COMMAND ${CMAKE_COMMAND} -E echo "Include dirs: ${INCLUDE_DIR};${SRC_DIR}"
)

message(STATUS "ExynosTools configured. Source count: ${ALL_C_SOURCES_JOINED}")
