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
set(BUILD_DIR "${CMAKE_BINARY_DIR}")
set(SHADER_BUILD_DIR "${BUILD_DIR}/shaders")
set(GENERATED_HEADER_DIR "${BUILD_DIR}/generated_includes")
set(GENERATED_EMBED_SRC "${BUILD_DIR}/shaders_embed.c")

include_directories("${INCLUDE_DIR}")
include_directories("${SRC_DIR}")
include_directories("${GENERATED_HEADER_DIR}")
include_directories("${BUILD_DIR}")

# Find external tools
find_program(GLSLANG_BIN glslangValidator)
find_program(XXD_BIN xxd)
find_program(HEXDUMP_BIN hexdump)

if(NOT GLSLANG_BIN)
  message(FATAL_ERROR "glslangValidator not found. Install glslang-tools or provide path.")
endif()
if(NOT XXD_BIN AND NOT HEXDUMP_BIN)
  message(FATAL_ERROR "Neither xxd nor hexdump found; install one of them.")
endif()

# Gather shader sources
file(GLOB_RECURSE SHADER_SOURCES_REL RELATIVE "${PROJ_ROOT}" "${SRC_DIR}/*.comp" "${SRC_DIR}/*.frag" "${SRC_DIR}/*.vert" "${SRC_DIR}/*.glsl")

set(SPIRV_OUTS "")
set(SPIRV_HEADERS "")

foreach(rel IN LISTS SHADER_SOURCES_REL)
  set(SRC_ABS "${PROJ_ROOT}/${rel}")
  get_filename_component(DIR_PART "${rel}" DIRECTORY)
  get_filename_component(NAME_WE "${rel}" NAME_WE)

  set(SPV_OUT "${SHADER_BUILD_DIR}/${DIR_PART}/${NAME_WE}.spv")
  list(APPEND SPIRV_OUTS "${SPV_OUT}")

  # create stable flattened header basename: dir_name_name_shader_spv.h
  string(REPLACE "/" "_" HDR_BASENAME "${DIR_PART}_${NAME_WE}_shader_spv.h")
  set(HEADER_OUT "${GENERATED_HEADER_DIR}/${HDR_BASENAME}")
  list(APPEND SPIRV_HEADERS "${HEADER_OUT}")

  add_custom_command(
    OUTPUT "${SPV_OUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<SHELL_PATH:${SHADER_BUILD_DIR}/${DIR_PART}>"
    COMMAND ${GLSLANG_BIN} -V "${SRC_ABS}" -o "${SPV_OUT}"
    DEPENDS "${SRC_ABS}"
    COMMENT "Compiling ${rel} -> ${SPV_OUT}"
    VERBATIM
  )

  if(XXD_BIN)
    # xxd -i path then normalize names to <NAME_WE>_shader_spv and <NAME_WE>_shader_spv_len
    add_custom_command(
      OUTPUT "${HEADER_OUT}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_HEADER_DIR}"
      COMMAND ${XXD_BIN} -i "${SPV_OUT}" > "${HEADER_OUT}.tmp"
      COMMAND ${CMAKE_COMMAND} -E env sh -c "sed -E -e 's/^unsigned char /const uint8_t /' -e 's/unsigned int (.*)_spv_len/const size_t \\1_shader_spv_len/' -e 's/unsigned char (.*)_spv\\[\\]/const uint8_t \\1_shader_spv[]/' '${HEADER_OUT}.tmp' > '${HEADER_OUT}'"
      COMMAND ${CMAKE_COMMAND} -E remove "${HEADER_OUT}.tmp"
      DEPENDS "${SPV_OUT}"
      COMMENT "Converting ${SPV_OUT} -> header ${HEADER_OUT} (xxd)"
      VERBATIM
    )
  else()
    # hexdump fallback: emit uint32_t arrays (portable) with naming <NAME_WE>_shader_spv
    add_custom_command(
      OUTPUT "${HEADER_OUT}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_HEADER_DIR}"
      COMMAND ${CMAKE_COMMAND} -E env sh -c "echo '#include <stddef.h>' > '${HEADER_OUT}'; echo '#include <stdint.h>' >> '${HEADER_OUT}'; echo 'const uint32_t ${NAME_WE}_shader_spv[] = {' >> '${HEADER_OUT}'; ${HEXDUMP_BIN} -v -e '4/4 \"0x%08x, \" \"\\\\n\"' '${SPV_OUT}' >> '${HEADER_OUT}'; echo '};' >> '${HEADER_OUT}'; echo 'const size_t ${NAME_WE}_shader_spv_len = (sizeof(${NAME_WE}_shader_spv)/sizeof(${NAME_WE}_shader_spv[0]));' >> '${HEADER_OUT}'"
      DEPENDS "${SPV_OUT}"
      COMMENT "Converting ${SPV_OUT} -> header ${HEADER_OUT} (hexdump)"
      VERBATIM
    )
  endif()
endforeach()

if(SPIRV_OUTS)
  add_custom_target(compile_spirv ALL DEPENDS ${SPIRV_OUTS})
endif()
if(SPIRV_HEADERS)
  add_custom_target(spv_to_c_headers ALL DEPENDS ${SPIRV_HEADERS})
endif()

# Generate a single C source that includes all generated headers
add_custom_command(
  OUTPUT "${GENERATED_EMBED_SRC}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${GENERATED_HEADER_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "/* Auto-generated: include shader headers */" > "${GENERATED_EMBED_SRC}"
  COMMAND ${CMAKE_COMMAND} -E env sh -c "for h in ${SPIRV_HEADERS}; do echo \"#include \\\"$(basename \\\"\\$h\\\")\\\"\" >> \"${GENERATED_EMBED_SRC}\"; done"
  DEPENDS ${SPIRV_HEADERS}
  COMMENT "Generating ${GENERATED_EMBED_SRC}"
  VERBATIM
)

add_custom_target(gen_shaders_embed ALL DEPENDS "${GENERATED_EMBED_SRC}")

# Compile embed source into object library
add_library(shaders_embed_obj OBJECT "${GENERATED_EMBED_SRC}")
add_dependencies(shaders_embed_obj gen_shaders_embed)
target_include_directories(shaders_embed_obj PRIVATE "${GENERATED_HEADER_DIR}")

# Gather C sources, exclude shader directories and selfcheck.c from main target
file(GLOB_RECURSE ALL_C_REL RELATIVE "${PROJ_ROOT}" "${SRC_DIR}/*.c")
set(MAIN_SOURCES "")
foreach(rel IN LISTS ALL_C_REL)
  string(FIND "${rel}" "/shaders/" _has_shaders_dir)
  string(FIND "${rel}" "/shader/" _has_shader_dir)
  string(FIND "${rel}" "selfcheck.c" _is_selfcheck)
  if(_has_shaders_dir EQUAL -1 AND _has_shader_dir EQUAL -1 AND _is_selfcheck EQUAL -1)
    list(APPEND MAIN_SOURCES "${PROJ_ROOT}/${rel}")
  endif()
endforeach()

if(NOT MAIN_SOURCES)
  message(FATAL_ERROR "No C sources found under ${SRC_DIR}")
endif()

# Optionally build logging.c as OBJECT to avoid symbol duplication if present
set(LOGGING_REL "src/logging.c")
set(LOGGING_ABS "${PROJ_ROOT}/${LOGGING_REL}")
set(HAVE_LOGGING OFF)
foreach(f IN LISTS MAIN_SOURCES)
  if(f STREQUAL "${LOGGING_ABS}")
    set(HAVE_LOGGING ON)
  endif()
endforeach()

if(HAVE_LOGGING)
  list(REMOVE_ITEM MAIN_SOURCES "${LOGGING_ABS}")
  add_library(logging_obj OBJECT "${LOGGING_ABS}")
  target_include_directories(logging_obj PRIVATE "${INCLUDE_DIR}" "${SRC_DIR}" "${GENERATED_HEADER_DIR}")
  target_compile_definitions(logging_obj PRIVATE XENO_LOG_IMPLEMENTATION_PRESENT=1)
  target_compile_options(logging_obj PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

# Main executable
add_executable(exynostools ${MAIN_SOURCES})
if(SPIRV_OUTS)
  add_dependencies(exynostools compile_spirv)
endif()
if(SPIRV_HEADERS)
  add_dependencies(exynostools spv_to_c_headers)
endif()
add_dependencies(exynostools gen_shaders_embed)
if(HAVE_LOGGING)
  target_sources(exynostools PRIVATE $<TARGET_OBJECTS:logging_obj>)
endif()
target_sources(exynostools PRIVATE $<TARGET_OBJECTS:shaders_embed_obj>)

target_include_directories(exynostools PRIVATE "${INCLUDE_DIR}" "${SRC_DIR}" "${GENERATED_HEADER_DIR}" "${BUILD_DIR}")
target_compile_options(exynostools PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
if(FAIL_ON_WARNINGS)
  target_compile_options(exynostools PRIVATE -Werror)
endif()
if(ENABLE_VERBOSE_BUILD)
  set(CMAKE_VERBOSE_MAKEFILE ON)
endif()

# If you want a selfcheck tool in future, keep selfcheck.c but not built by default.
# (You can add it manually as a separate target when needed.)

# Vulkan and threads linking
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
endif()

find_package(Threads QUIET)
if(THREADS_FOUND)
  target_link_libraries(exynostools PRIVATE Threads::Threads)
endif()

target_compile_definitions(exynostools PRIVATE $<$<CONFIG:Debug>:DEBUG=1>)
install(TARGETS exynostools RUNTIME DESTINATION bin)

# Debug helpers
string(REPLACE ";" " " SOURCES_JOINED "${MAIN_SOURCES}")
add_custom_target(show_sources COMMENT "Show discovered C sources" COMMAND ${CMAKE_COMMAND} -E echo "${SOURCES_JOINED}")
add_custom_target(show_configuration
  COMMENT "Show configuration"
  COMMAND ${CMAKE_COMMAND} -E echo "Build dir: ${BUILD_DIR}; Generated headers: ${GENERATED_HEADER_DIR}; Shader build dir: ${SHADER_BUILD_DIR}"
)
