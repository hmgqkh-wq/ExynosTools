cmake_minimum_required(VERSION 3.13)
project(ExynosTools C)

# Language and standard
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
set(SHADER_SRC_DIR "${SRC_DIR}/shaders")
set(SHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/shaders")
set(GENERATED_HEADER_DIR "${CMAKE_BINARY_DIR}/generated_includes")

# Ensure generated include dir is available to all TUs
include_directories(${GENERATED_HEADER_DIR})

# ----------------------------------------------------------------------------
# 1) Discover shader files (search common locations under src)
# ----------------------------------------------------------------------------
file(GLOB_RECURSE SHADER_SOURCES_RAW_REL
  RELATIVE "${PROJ_ROOT}"
  "${SRC_DIR}/*.comp"
  "${SRC_DIR}/*.comp.glsl"
  "${SRC_DIR}/*.comp.hlsl"
  "${SRC_DIR}/*.comp.glslinc"
  "${SRC_DIR}/*.frag"
  "${SRC_DIR}/*.vert"
  "${SRC_DIR}/*.glsl"
)

# Normalize list and build commands
set(SPIRV_OUTPUTS "")
set(SPIRV_TO_C_HEADERS "")

foreach(relpath ${SHADER_SOURCES_RAW_REL})
  # Input absolute path
  set(SRC_ABS "${PROJ_ROOT}/${relpath}")

  # Compute output SPV path inside build tree preserving directory structure
  get_filename_component(dirpart "${relpath}" DIRECTORY)
  get_filename_component(name_we "${relpath}" NAME_WE)
  set(SPV_OUT "${SHADER_BUILD_DIR}/${dirpart}/${name_we}.spv")
  list(APPEND SPIRV_OUTPUTS "${SPV_OUT}")

  # Compute generated C header path for embedding SPV into C array
  # Header name: <name_we>_spv.h placed under GENERATED_HEADER_DIR/<dirpart>/
  string(REPLACE "/" "_" hdrname "${dirpart}/${name_we}_spv.h")
  set(HEADER_OUT "${GENERATED_HEADER_DIR}/${hdrname}")
  list(APPEND SPIRV_TO_C_HEADERS "${HEADER_OUT}")

  # Ensure output directories are created before running tools
  add_custom_command(
    OUTPUT "${SPV_OUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<SHELL_PATH:${SHADER_BUILD_DIR}/${dirpart}>"
    COMMAND ${CMAKE_COMMAND} -E echo "Compiling shader ${SRC_ABS} -> ${SPV_OUT}"
    COMMAND glslangValidator -V "${SRC_ABS}" -o "${SPV_OUT}"
    DEPENDS "${SRC_ABS}"
    COMMENT "Compiling shader to SPIR-V: ${relpath}"
    VERBATIM
  )

  # Convert SPV to C header (xxd or hexdump) — produce header containing:
  #   extern const uint32_t <name>_spv[]; extern const size_t <name>_spv_len;
  # and an array initializer of 32-bit words.
  add_custom_command(
    OUTPUT "${HEADER_OUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<SHELL_PATH:${GENERATED_HEADER_DIR}>"
    COMMAND ${CMAKE_COMMAND} -E echo "Generating C header ${HEADER_OUT} from ${SPV_OUT}"
    COMMAND ${CMAKE_COMMAND} -E sleep 0  # placeholder to ensure command list non-empty
    # The actual conversion uses a small CMake-invoked script below (spv_to_c.sh). We'll call it.
    COMMAND ${CMAKE_COMMAND} -E env SHADER_IN="${SPV_OUT}" HEADER_OUT="${HEADER_OUT}" NAME_WE="${name_we}" bash "${CMAKE_SOURCE_DIR}/cmake/spv_to_c.sh"
    DEPENDS "${SPV_OUT}"
    COMMENT "Converting SPIR-V to C header: ${name_we}"
    VERBATIM
  )
endforeach()

# If any shaders discovered, create top-level targets
if(SPIRV_OUTPUTS)
  add_custom_target(compile_spirv ALL DEPENDS ${SPIRV_OUTPUTS})
endif()

if(SPIRV_TO_C_HEADERS)
  add_custom_target(spv_to_c_headers ALL DEPENDS ${SPIRV_TO_C_HEADERS})
endif()

# Ensure headers are generated before compiling C sources that include them by
# making the generated header target order before the executable (we'll add explicit dependency later)

# ----------------------------------------------------------------------------
# 2) Gather only .c sources and exclude shader files/assets
# ----------------------------------------------------------------------------
file(GLOB_RECURSE ALL_C_SOURCES_RAW_REL
    RELATIVE "${PROJ_ROOT}"
    "${SRC_DIR}/*.c"
)

set(ALL_C_SOURCES "")
foreach(rel ${ALL_C_SOURCES_RAW_REL})
  # Exclude anything in shader dirs or explicit shader extensions accidentally named .c
  string(FIND "${rel}" "/shaders/" _has_shaders_dir)
  string(FIND "${rel}" "/shader/" _has_shader_dir)
  string(FIND "${rel}" "/assets/" _has_assets_dir)
  if(_has_shaders_dir EQUAL -1 AND _has_shader_dir EQUAL -1 AND _has_assets_dir EQUAL -1)
    list(APPEND ALL_C_SOURCES "${PROJ_ROOT}/${rel}")
  endif()
endforeach()

if(NOT ALL_C_SOURCES)
  message(FATAL_ERROR "No C source files found under ${SRC_DIR} — check repository layout")
endif()

# ----------------------------------------------------------------------------
# 3) Logging TU special-case: compile src/logging.c as OBJECT with XENO_LOG_IMPLEMENTATION_PRESENT
# ----------------------------------------------------------------------------
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
  target_include_directories(logging_obj PRIVATE "${INCLUDE_DIR}" "${SRC_DIR}" "${GENERATED_HEADER_DIR}")
  target_compile_definitions(logging_obj PRIVATE XENO_LOG_IMPLEMENTATION_PRESENT=1)
  target_compile_options(logging_obj PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# ----------------------------------------------------------------------------
# 4) Add executable and wire shader header dependency
# ----------------------------------------------------------------------------
add_executable(exynostools ${MAIN_SOURCES})

# Ensure generated headers are created before compiling the exe (pre-build dependency)
if(SPIRV_TO_C_HEADERS)
  add_dependencies(exynostools spv_to_c_headers)
endif()
if(SPIRV_OUTPUTS)
  add_dependencies(exynostools compile_spirv)
endif()

if(HAVE_LOGGING_SRC)
  target_sources(exynostools PRIVATE $<TARGET_OBJECTS:logging_obj>)
endif()

target_include_directories(exynostools
    PRIVATE
        "${INCLUDE_DIR}"
        "${SRC_DIR}"
        "${GENERATED_HEADER_DIR}"
)

# ----------------------------------------------------------------------------
# 5) Compiler flags and options
# ----------------------------------------------------------------------------
target_compile_options(exynostools PRIVATE
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
)
if(FAIL_ON_WARNINGS)
    target_compile_options(exynostools PRIVATE -Werror)
endif()
if(ENABLE_VERBOSE_BUILD)
    set(CMAKE_VERBOSE_MAKEFILE ON)
endif()

# ----------------------------------------------------------------------------
# 6) Platform detection, Vulkan, Threads
# ----------------------------------------------------------------------------
set(TARGET_IS_AARCH64 FALSE)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_C_COMPILER_TARGET MATCHES "aarch64")
    set(TARGET_IS_AARCH64 TRUE)
endif()

if(TARGET_IS_AARCH64)
    message(STATUS "AArch64 target detected; attempting to apply AArch64-specific linker flags if supported")
    # If toolchain rejects it you can remove the following line; keep it only when toolchain supports it.
    target_link_options(exynostools PRIVATE "--fix-cortex-a53-843419")
endif()

find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
else()
    message(STATUS "Vulkan not found by CMake; link may fail if Vulkan symbols used")
endif()

find_package(Threads QUIET)
if(THREADS_FOUND)
    target_link_libraries(exynostools PRIVATE Threads::Threads)
endif()

target_compile_definitions(exynostools PRIVATE
    $<$<CONFIG:Debug>:DEBUG=1>
)

install(TARGETS exynostools RUNTIME DESTINATION bin)

# ----------------------------------------------------------------------------
# 7) Safe debug helpers
# ----------------------------------------------------------------------------
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
  COMMAND ${CMAKE_COMMAND} -E echo "Include dirs: ${INCLUDE_DIR};${SRC_DIR};${GENERATED_HEADER_DIR}"
)

message(STATUS "ExynosTools configured. Discovered shaders: ${SHADER_SOURCES_RAW_REL}")
message(STATUS "Generated header dir: ${GENERATED_HEADER_DIR}")
