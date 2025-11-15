cmake_minimum_required(VERSION 3.15)
project(exynostools C)

option(BUILD_SHADERS "Compile GLSL shaders to SPIR-V and embed them into C sources" ON)
option(BUILD_SELFTEST "Build the selfcheck program (defines its own main). OFF prevents duplicate main." OFF)
option(ENABLE_LTO "Enable link-time optimization where supported" ON)
set(SHADER_TARGET_ENV "vulkan1.3" CACHE STRING "SPIR-V target environment")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(DRIVERS_XCLIPSE_DIR "${SRC_DIR}/drivers/xclipse")
set(SHADERS_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SHADER_DIR "${CMAKE_BINARY_DIR}/generated_shaders")
file(MAKE_DIRECTORY "${GENERATED_SHADER_DIR}")

set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
include_directories("${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(GLSLC_EXEC glslc)
find_program(XXD_EXEC xxd)

if(BUILD_SHADERS)
  if(NOT XXD_EXEC)
    message(FATAL_ERROR "Required tool 'xxd' not found.")
  endif()
  if(NOT GLSLANG_VALIDATOR AND NOT GLSLC_EXEC)
    message(FATAL_ERROR "No shader compiler found.")
  endif()
endif()

set(GENERATED_SHADER_C_FILES)
set(GENERATED_SHADER_SPV_FILES)

if(EXISTS "${SHADERS_SRC_DIR}")
  file(GLOB_RECURSE _SHADER_FILES
    "${SHADERS_SRC_DIR}/*.comp"
    "${SHADERS_SRC_DIR}/*.frag"
    "${SHADERS_SRC_DIR}/*.vert"
    "${SHADERS_SRC_DIR}/*.glsl"
    "${SHADERS_SRC_DIR}/*.spv"
  )

  foreach(_infile IN LISTS _SHADER_FILES)
    get_filename_component(_base ${_infile} NAME_WE)
    get_filename_component(_ext  ${_infile} EXT)
    string(TOLOWER "${_ext}" _ext_l)

    set(_sanitized "${GENERATED_SHADER_DIR}/${_base}${_ext_l}")
    set(_out_spv    "${GENERATED_SHADER_DIR}/${_base}.spv")
    set(_out_c      "${GENERATED_SHADER_DIR}/${_base}_spv.c")

    file(READ "${_infile}" _raw_content)
    string(REPLACE "\r\n" "\n" _content "${_raw_content}")
    string(REPLACE "\r" "\n" _content "${_content}")
    if(NOT _content STREQUAL "")
      string(REGEX REPLACE "^[ \t\n\r]*" "" _content "${_content}")
    endif()

    string(REGEX MATCH "^[^\n]*" _first_line "${_content}")
    string(REGEX MATCH "^[[:space:]]*#version[[:space:]]+[0-9]+" _has_version "${_first_line}")
    if(_has_version)
      string(REGEX REPLACE "^[^\n]*\n" "" _body "${_content}")
      file(WRITE "${_sanitized}" "#version 450\n${_body}")
    else()
      file(WRITE "${_sanitized}" "#version 450\n${_content}")
    endif()

    if(_ext_l STREQUAL ".comp")
      set(_stage "comp")
    elseif(_ext_l STREQUAL ".frag")
      set(_stage "frag")
    elseif(_ext_l STREQUAL ".vert")
      set(_stage "vert")
    else()
      set(_stage "include")
    endif()

    set(_has_main "")
    if(NOT _stage STREQUAL "include")
      file(READ "${_sanitized}" _san_content)
      string(REGEX MATCH "void[[:space:]]+main[[:space:]]*\\(" _has_main "${_san_content}")
      if(NOT _has_main)
        string(REGEX MATCH "[[:space:]]main[[:space:]]*\\(" _has_main "${_san_content}")
      endif()
    endif()

    if(BUILD_SHADERS)
      if(_stage STREQUAL "include")
        add_custom_command(
          OUTPUT "${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E touch "${_out_spv}"
          DEPENDS "${_sanitized}"
        )
      elseif(_has_main)
        if(GLSLANG_VALIDATOR)
          add_custom_command(
            OUTPUT "${_out_spv}"
            COMMAND ${GLSLANG_VALIDATOR} -V --target-env ${SHADER_TARGET_ENV} -S ${_stage} -o "${_out_spv}" "${_sanitized}"
            DEPENDS "${_sanitized}"
          )
        else()
          add_custom_command(
            OUTPUT "${_out_spv}"
            COMMAND ${GLSLC_EXEC} -fshader-stage=${_stage} --target-env=${SHADER_TARGET_ENV} "${_sanitized}" -o "${_out_spv}"
            DEPENDS "${_sanitized}"
          )
        endif()
      else()
        add_custom_command(
          OUTPUT "${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E touch "${_out_spv}"
          DEPENDS "${_sanitized}"
        )
      endif()
    else()
      add_custom_command(
        OUTPUT "${_out_spv}"
        COMMAND ${CMAKE_COMMAND} -E touch "${_out_spv}"
        DEPENDS "${_sanitized}"
      )
    endif()

    add_custom_command(
      OUTPUT "${_out_c}"
      COMMAND ${XXD_EXEC} -i "${_out_spv}" "${_out_c}"
      DEPENDS "${_out_spv}"
    )

    list(APPEND GENERATED_SHADER_C_FILES "${_out_c}")
    list(APPEND GENERATED_SHADER_SPV_FILES "${_out_spv}")
  endforeach()
endif()

if(GENERATED_SHADER_C_FILES)
  add_custom_target(exy_generate_shaders DEPENDS ${GENERATED_SHADER_C_FILES})
else()
  add_custom_target(exy_generate_shaders)
endif()

file(GLOB_RECURSE PROJECT_SRCS "${SRC_DIR}/*.c")

set(_selfcheck_path "${SRC_DIR}/selfcheck.c")
if(NOT BUILD_SELFTEST AND EXISTS "${_selfcheck_path}")
  list(REMOVE_ITEM PROJECT_SRCS "${_selfcheck_path}")
endif()

set(_conflict_files
  "${SRC_DIR}/shaders_embed_stub.c"
  "${SRC_DIR}/shaders_embed.c"
  "${SRC_DIR}/xeno_log_stream.c"
  "${SRC_DIR}/xeno_log_stream_stub.c"
  "${SRC_DIR}/xeno_wrapper_stubs.c"
)
foreach(_cf IN LISTS _conflict_files)
  if(EXISTS "${_cf}")
    list(REMOVE_ITEM PROJECT_SRCS "${_cf}")
  endif()
endforeach()

set(XCLIPSE_SRCS "")
if(EXISTS "${DRIVERS_XCLIPSE_DIR}")
  file(GLOB_RECURSE XCLIPSE_SRCS "${DRIVERS_XCLIPSE_DIR}/*.c" "${DRIVERS_XCLIPSE_DIR}/*.h")
endif()

add_library(xeno_wrapper SHARED ${XCLIPSE_SRCS})
add_dependencies(xeno_wrapper exy_generate_shaders)
target_include_directories(xeno_wrapper PRIVATE "${INCLUDE_DIR}" "${DRIVERS_XCLIPSE_DIR}" "${GENERATED_SHADER_DIR}")
if(GENERATED_SHADER_C_FILES)
  target_sources(xeno_wrapper PRIVATE ${GENERATED_SHADER_C_FILES})
endif()

set_target_properties(xeno_wrapper PROPERTIES OUTPUT_NAME "xeno_wrapper")

if(NOT MSVC)
  target_compile_options(xeno_wrapper PRIVATE -fPIC -fvisibility=hidden -O3 -Wall -Wextra -Wno-unused-parameter)
endif()

find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  target_include_directories(xeno_wrapper PRIVATE ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(xeno_wrapper PRIVATE ${Vulkan_LIBRARIES})
else()
  find_library(LIBVULKAN_NEEDED NAMES vulkan libvulkan vulkan-1)
  if(LIBVULKAN_NEEDED)
    target_link_libraries(xeno_wrapper PRIVATE ${LIBVULKAN_NEEDED})
  endif()
endif()

target_compile_definitions(xeno_wrapper PRIVATE XENO_EXPORT_INIT_SYMBOL=xeno_init)

add_executable(exynostools ${PROJECT_SRCS})
add_dependencies(exynostools exy_generate_shaders)
target_include_directories(exynostools PRIVATE "${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")
if(NOT MSVC)
  target_compile_options(exynostools PRIVATE -O3 -Wall -Wextra -Wno-unused-parameter)
endif()

if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(exynostools PRIVATE ${CMAKE_THREAD_LIBS_INIT})
  find_library(LIBDL dl)
  if(LIBDL)
    target_link_libraries(exynostools PRIVATE ${LIBDL})
    target
