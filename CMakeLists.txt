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

include_directories(${GENERATED_HEADER_DIR})
include_directories(${INCLUDE_DIR})
include_directories(${SRC_DIR})

# ----------------------------------------------------------------------------
# Discover shader sources and compile to SPIR-V, then to C headers
# ----------------------------------------------------------------------------
file(GLOB_RECURSE SHADER_SOURCES_RAW_REL
  RELATIVE "${PROJ_ROOT}"
  "${SRC_DIR}/*.comp"
  "${SRC_DIR}/*.frag"
  "${SRC_DIR}/*.vert"
  "${SRC_DIR}/*.glsl"
)

set(SPIRV_OUTPUTS "")
set(SPIRV_HEADER_OUTPUTS "")
set(SPIRV_HEADER_BASENAMES "")

# Always use hexdump to generate stable identifiers: <NAME>_shader_spv and <NAME>_shader_spv_len
function(add_spv_to_c_rule SPV_IN HEADER_OUT NAME_WE)
  get_filename_component(HEADER_DIR "${HEADER_OUT}" DIRECTORY)
  add_custom_command(
    OUTPUT "${HEADER_OUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${HEADER_DIR}"
    COMMAND /bin/sh -c "
      echo '#include <stddef.h>' > '${HEADER_OUT}';
      echo '#include <stdint.h>' >> '${HEADER_OUT}';
      echo 'const uint32_t ${NAME_WE}_shader_spv[] = {' >> '${HEADER_OUT}';
      hexdump -v -e '4/4 \"0x%08x, \" \"\\n\"' '${SPV_IN}' >> '${HEADER_OUT}';
      echo '};' >> '${HEADER_OUT}';
      echo 'const size_t ${NAME_WE}_shader_spv_len = (sizeof(${NAME_WE}_shader_spv)/sizeof(${NAME_WE}_shader_spv[0]));' >> '${HEADER_OUT}';
    "
    DEPENDS "${SPV_IN}"
    COMMENT "Converting SPIR-V ${SPV_IN} -> header ${HEADER_OUT}"
    VERBATIM
  )
endfunction()

foreach(relpath IN LISTS SHADER_SOURCES_RAW_REL)
  set(SRC_ABS "${PROJ_ROOT}/${relpath}")
  get_filename_component(DIR_PART "${relpath}" DIRECTORY)
  get_filename_component(NAME_WE "${relpath}" NAME_WE)

  # Compile GLSL -> SPIR-V
  set(SPV_OUT "${SHADER_BUILD_DIR}/${DIR_PART}/${NAME_WE}.spv")
  list(APPEND SPIRV_OUTPUTS "${SPV_OUT}")
  add_custom_command(
    OUTPUT "${SPV_OUT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<SHELL_PATH:${SHADER_BUILD_DIR}/${DIR_PART}>"
    COMMAND glslangValidator -V "${SRC_ABS}" -o "${SPV_OUT}"
    DEPENDS "${SRC_ABS}"
    COMMENT "Compiling shader to SPIR-V: ${relpath}"
    VERBATIM
  )

  # Generate a flat header name so we can include it easily from the build dir
  string(REPLACE "/" "_" HDR_BASENAME "${DIR_PART}_${NAME_WE}_shader_spv.h")
  set(HEADER_OUT "${GENERATED_HEADER_DIR}/${HDR_BASENAME}")
  list(APPEND SPIRV_HEADER_OUTPUTS "${HEADER_OUT}")
  list(APPEND SPIRV_HEADER_BASENAMES "${HDR_BASENAME}")

  add_spv_to_c_rule("${SPV_OUT}" "${HEADER_OUT}" "${NAME_WE}")
endforeach()

if(SPIRV_OUTPUTS)
  add_custom_target(compile_spirv ALL DEPENDS ${SPIRV_OUTPUTS})
endif()
if(SPIRV_HEADER_OUTPUTS)
  add_custom_target(spv_to_c_headers ALL DEPENDS ${SPIRV_HEADER_OUTPUTS})
endif()

# ----------------------------------------------------------------------------
# Autogenerate a single source file that instantiates all shader arrays
# (prevents multiple definition and ensures symbols exist for bc_emulate.c)
# ----------------------------------------------------------------------------
# Generate shaders_embed.c with includes for all headers
file(WRITE "${GENERATED_EMBED_SRC}" "/* Auto-generated: instantiates shader arrays */\n")
foreach(HDR IN LISTS SPIRV_HEADER_BASENAMES)
  file(APPEND "${GENERATED_EMBED_SRC}" "#include \"${HDR}\"\n")
endforeach()

# Create an object library from the generated embed source
add_library(shaders_embed_obj OBJECT "${GENERATED_EMBED_SRC}")
add_dependencies(shaders_embed_obj spv_to_c_headers)
target_include_directories(shaders_embed_obj PRIVATE "${GENERATED_HEADER_DIR}")

# ----------------------------------------------------------------------------
# Discover C sources (exclude assets/shaders and selfcheck.c from main target)
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
# logging.c as OBJECT (so main can link without duplicate TU symbols)
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
  target_include_directories(logging_obj PRIVATE "${INCLUDE_DIR}" "${SRC_DIR}" "${GENERATED_HEADER_DIR}")
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
if(SPIRV_HEADER_OUTPUTS)
  add_dependencies(exynostools spv_to_c_headers)
endif()
if(HAVE_LOGGING_SRC)
  target_sources(exynostools PRIVATE $<TARGET_OBJECTS:logging_obj>)
endif()
# Link shader instantiations
target_sources(exynostools PRIVATE $<TARGET_OBJECTS:shaders_embed_obj>)

target_include_directories(exynostools
    PRIVATE
        "${INCLUDE_DIR}"
        "${SRC_DIR}"
        "${GENERATED_HEADER_DIR}"
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
target_include_directories(exynostools_selfcheck PRIVATE "${INCLUDE_DIR}" "${SRC_DIR}" "${GENERATED_HEADER_DIR}")
target_compile_options(exynostools_selfcheck PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
if(SPIRV_OUTPUTS)
  add_dependencies(exynostools_selfcheck compile_spirv)
endif()
if(SPIRV_HEADER_OUTPUTS)
  add_dependencies(exynostools_selfcheck spv_to_c_headers)
endif()
# Link shader instantiations
target_sources(exynostools_selfcheck PRIVATE $<TARGET_OBJECTS:shaders_embed_obj>)

# Vulkan and Threads
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
  COMMAND ${CMAKE_COMMAND} -E echo "Include dirs: ${INCLUDE_DIR};${SRC_DIR};${GENERATED_HEADER_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "Shader build dir: ${SHADER_BUILD_DIR}"
  COMMAND ${CMAKE_COMMAND} -E echo "Generated header dir: ${GENERATED_HEADER_DIR}"
)
