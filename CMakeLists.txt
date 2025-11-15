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
set(SHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/shaders")
set(GENERATED_HEADER_DIR "${CMAKE_BINARY_DIR}/generated_includes")
set(GENERATED_EMBED_SRC "${CMAKE_BINARY_DIR}/shaders_embed.c")
set(GENERATOR_SCRIPT "${CMAKE_BINARY_DIR}/generate_shaders_embed.sh")

include_directories("${GENERATED_HEADER_DIR}")
include_directories("${INCLUDE_DIR}")
include_directories("${SRC_DIR}")
include_directories("${CMAKE_BINARY_DIR}") # allow includes from generated files

# ----------------------------------------------------------------------------
# Discover shader sources and compile to SPIR-V
# ----------------------------------------------------------------------------
file(GLOB_RECURSE SHADER_SOURCES_RAW_REL
  RELATIVE "${PROJ_ROOT}"
  "${SRC_DIR}/*.comp"
  "${SRC_DIR}/*.frag"
  "${SRC_DIR}/*.vert"
  "${SRC_DIR}/*.glsl"
)

set(SPIRV_OUTPUTS "")
set(SPIRV_NAMES "")

foreach(relpath IN LISTS SHADER_SOURCES_RAW_REL)
  set(SRC_ABS "${PROJ_ROOT}/${relpath}")
  get_filename_component(DIR_PART "${relpath}" DIRECTORY)
  get_filename_component(NAME_WE "${relpath}" NAME_WE)

  # SPV output path (mirror directory structure under build/shaders)
  set(SPV_OUT "${SHADER_BUILD_DIR}/${DIR_PART}/${NAME_WE}.spv")
  list(APPEND SPIRV_OUTPUTS "${SPV_OUT}")
  list(APPEND SPIRV_NAMES "${NAME_WE}")

  add_custom_command(
    OUTPUT "${SPV_OUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<SHELL_PATH:${SHADER_BUILD_DIR}/${DIR_PART}>"
    COMMAND glslangValidator -V "${SRC_ABS}" -o "${SPV_OUT}"
    DEPENDS "${SRC_ABS}"
    COMMENT "Compiling shader to SPIR-V: ${relpath}"
    VERBATIM
  )
endforeach()

if(SPIRV_OUTPUTS)
  add_custom_target(compile_spirv ALL DEPENDS ${SPIRV_OUTPUTS})
endif()

# ----------------------------------------------------------------------------
# Write a small generator script that will produce a single C file embedding all SPIR-V arrays
# This avoids complex quoting in add_custom_command and keeps CMake parsing safe.
# ----------------------------------------------------------------------------
file(WRITE "${GENERATOR_SCRIPT}" "#!/usr/bin/env bash\n")
file(APPEND "${GENERATOR_SCRIPT}" "set -euo pipefail\n")
file(APPEND "${GENERATOR_SCRIPT}" "OUT_FILE=\"${GENERATED_EMBED_SRC}\"\n")
file(APPEND "${GENERATOR_SCRIPT}" "echo '/* Auto-generated: embedded SPIR-V arrays */' > \"${GENERATED_EMBED_SRC}\"\n")
file(APPEND "${GENERATOR_SCRIPT}" "echo '#include <stddef.h>' >> \"${GENERATED_EMBED_SRC}\"\n")
file(APPEND "${GENERATOR_SCRIPT}" "echo '#include <stdint.h>' >> \"${GENERATED_EMBED_SRC}\"\n")
# Loop over SPIRV_OUTPUTS at generation time by expanding a CMake-provided literal list into the script
foreach(spv IN LISTS SPIRV_OUTPUTS)
  # escape any $ for safe append
  file(APPEND "${GENERATOR_SCRIPT}"
    "if [ -f \"${spv}\" ]; then\n"
    "  NAME=\"$(basename \"${spv}\" .spv)\"\n"
    "  echo \"/* embedding: ${spv} -> ${NAME}_shader_spv */\" >> \"${GENERATED_EMBED_SRC}\"\n"
    "  echo \"const uint32_t ${NAME}_shader_spv[] = {\" >> \"${GENERATED_EMBED_SRC}\"\n"
    "  hexdump -v -e '4/4 \"0x%08x, \" \"\\\\n\"' \"${spv}\" >> \"${GENERATED_EMBED_SRC}\"\n"
    "  echo \"};\" >> \"${GENERATED_EMBED_SRC}\"\n"
    "  echo \"const size_t ${NAME}_shader_spv_len = (sizeof(${NAME}_shader_spv)/sizeof(${NAME}_shader_spv[0]));\" >> \"${GENERATED_EMBED_SRC}\"\n"
    "else\n"
    "  NAME=\"$(basename \"${spv}\" .spv)\"\n"
    "  echo \"/* missing spv: ${spv} */\" >> \"${GENERATED_EMBED_SRC}\"\n"
    "  echo \"const uint32_t ${NAME}_shader_spv[] = {0};\" >> \"${GENERATED_EMBED_SRC}\"\n"
    "  echo \"const size_t ${NAME}_shader_spv_len = 0;\" >> \"${GENERATED_EMBED_SRC}\"\n"
    "fi\n"
  )
endforeach()
# Make generator script executable
file(APPEND "${GENERATOR_SCRIPT}" "chmod +x \"${GENERATOR_SCRIPT}\" || true\n")

# ----------------------------------------------------------------------------
# Use the generator script in an add_custom_command to create GENERATED_EMBED_SRC
# ----------------------------------------------------------------------------
add_custom_command(
  OUTPUT "${GENERATED_EMBED_SRC}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}"
  COMMAND ${CMAKE_COMMAND} -E env bash "${GENERATOR_SCRIPT}"
  DEPENDS ${SPIRV_OUTPUTS}
  COMMENT "Generating C source with embedded SPIR-V arrays: ${GENERATED_EMBED_SRC}"
  VERBATIM
)

add_custom_target(gen_shaders_embed ALL DEPENDS "${GENERATED_EMBED_SRC}")
add_library(shaders_embed_obj OBJECT "${GENERATED_EMBED_SRC}")
add_dependencies(shaders_embed_obj gen_shaders_embed)
target_include_directories(shaders_embed_obj PRIVATE "${CMAKE_BINARY_DIR}")

# ----------------------------------------------------------------------------
# Discover C sources (exclude shader files and selfcheck.c from main target)
# ----------------------------------------------------------------------------
file(GLOB_RECURSE ALL_C_SOURCES_RAW_REL
    RELATIVE "${PROJ_ROOT}"
    "${SRC_DIR}/*.c"
)

set(ALL_C_SOURCES "")
foreach(rel IN LISTS ALL_C_SOURCES_RAW_REL)
  string(FIND "${rel}" "/shaders/" _has_shaders_dir)
  string(FIND "${rel}" "/shader/" _has_shader_dir)
  string(FIND "${rel}" "/assets/" _has_assets_dir)
  string(FIND "${rel}" "selfcheck.c" _is_selfcheck)
  if(_has_shaders_dir EQUAL -1 AND _has_shader_dir EQUAL -1 AND _has_assets_dir EQUAL -1 AND _is_selfcheck EQUAL -1)
    list(APPEND ALL_C_SOURCES "${PROJ_ROOT}/${rel}")
  endif()
endforeach()

if(NOT ALL_C_SOURCES)
  message(FATAL_ERROR "No C source files found under ${SRC_DIR}")
endif()

# ----------------------------------------------------------------------------
# logging.c as OBJECT to allow controlling symbol presence and avoid duplicates
# ----------------------------------------------------------------------------
set(LOGGING_REL_PATH "src/logging.c")
set(LOGGING_ABS_PATH "${PROJ_ROOT}/${LOGGING_REL_PATH}")
set(HAVE_LOGGING_SRC OFF)
foreach(f IN LISTS ALL_C_SOURCES)
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

# ----------------------------------------------------------------------------
# Main executable
# ----------------------------------------------------------------------------
add_executable(exynostools ${MAIN_SOURCES})
if(SPIRV_OUTPUTS)
  add_dependencies(exynostools compile_spirv)
endif()
add_dependencies(exynostools gen_shaders_embed)
if(HAVE_LOGGING_SRC)
  target_sources(exynostools PRIVATE $<TARGET_OBJECTS:logging_obj>)
endif()
target_sources(exynostools PRIVATE $<TARGET_OBJECTS:shaders_embed_obj>)

target_include_directories(exynostools
    PRIVATE
        "${INCLUDE_DIR}"
        "${SRC_DIR}"
        "${CMAKE_BINARY_DIR}"
)

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
# Self-check executable (isolated)
# ----------------------------------------------------------------------------
add_executable(exynostools_selfcheck "${SRC_DIR}/selfcheck.c")
if(SPIRV_OUTPUTS)
  add_dependencies(exynostools_selfcheck compile_spirv)
endif()
add_dependencies(exynostools_selfcheck gen_shaders_embed)
target_sources(exynostools_selfcheck PRIVATE $<TARGET_OBJECTS:shaders_embed_obj>)

target_include_directories(exynostools_selfcheck
    PRIVATE
      "${INCLUDE_DIR}"
      "${SRC_DIR}"
      "${CMAKE_BINARY_DIR}"
)

# ----------------------------------------------------------------------------
# Vulkan and Threads
# ----------------------------------------------------------------------------
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
  target_include_directories(exynostools_selfcheck PRIVATE ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(exynostools_selfcheck PRIVATE ${Vulkan_LIBRARIES})
endif()

find_package(Threads QUIET)
if(THREADS_FOUND)
  target_link_libraries(exynostools PRIVATE Threads::Threads)
  target_link_libraries(exynostools_selfcheck PRIVATE Threads::Threads)
endif()

target_compile_definitions(exynostools PRIVATE $<$<CONFIG:Debug>:DEBUG=1>)
target_compile_definitions(exynostools_selfcheck PRIVATE $<$<CONFIG:Debug>:DEBUG=1>)

install(TARGETS exynostools RUNTIME DESTINATION bin)
install(TARGETS exynostools_selfcheck RUNTIME DESTINATION bin)

# ----------------------------------------------------------------------------
# Debug helpers
# ----------------------------------------------------------------------------
string(REPLACE ";" " " ALL_C_SOURCES_JOINED "${ALL_C_SOURCES}")
add_custom_target(show_sources
  COMMENT "Printing discovered C sources"
  COMMAND ${CMAKE_COMMAND} -E echo "${ALL_C_SOURCES_JOINED}"
)

add_custom_target(show_configuration
  COMMENT "Project configuration summary"
  COMMAND ${CMAKE_COMMAND} -E echo "CMake generator: ${CMAKE_GENERATOR}"
  COMMAND ${CMAKE_COMMAND} -E echo "Build type: ${CMAKE_BUILD_TYPE}"
  COMMAND ${CMAKE_COMMAND} -E echo "Include dirs: ${INCLUDE_DIR};${SRC_DIR};${CMAKE_BINARY_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "Shader build dir: ${SHADER_BUILD_DIR}"
)
