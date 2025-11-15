cmake_minimum_required(VERSION 3.15)
project(exynostools C)

# ------------------------ User options ---------------------------------
option(BUILD_SHADERS "Compile GLSL shaders to SPIR-V and embed into C sources" ON)
option(ENABLE_LTO "Enable link-time optimization where supported" ON)
set(SHADER_TARGET_ENV "vulkan1.3" CACHE STRING "SPIR-V target environment (vulkan1.0|vulkan1.1|vulkan1.2|vulkan1.3)")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ------------------------ Paths and layout ------------------------------
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(SHADERS_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SHADER_DIR "${CMAKE_BINARY_DIR}/generated_shaders")
file(MAKE_DIRECTORY "${GENERATED_SHADER_DIR}")

include_directories("${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

# ------------------------ Find required tools ---------------------------
find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(GLSLC_EXEC glslc)
find_program(XXD_EXEC xxd)
find_program(BASH_EXEC bash)

if(BUILD_SHADERS)
  if(NOT XXD_EXEC)
    message(FATAL_ERROR "Required tool 'xxd' not found. Install (e.g. vim-common) or set -DBUILD_SHADERS=OFF and provide prebuilt .spv files.")
  endif()
  if(NOT GLSLANG_VALIDATOR AND NOT GLSLC_EXEC)
    message(FATAL_ERROR "No shader compiler found (glslangValidator or glslc). Install one or set -DBUILD_SHADERS=OFF.")
  endif()
endif()

# ------------------------ Optional repo normalizer ----------------------
set(REPO_NORMALIZER "${CMAKE_SOURCE_DIR}/normalize_and_validate_shaders.sh")
if(BUILD_SHADERS AND EXISTS "${REPO_NORMALIZER}")
  if(NOT BASH_EXEC)
    message(WARNING "normalize_and_validate_shaders.sh found but 'bash' is missing; skipping it.")
    add_custom_target(exy_repo_normalize_shaders)
  else()
    if(NOT TARGET exy_repo_normalize_shaders)
      add_custom_target(exy_repo_normalize_shaders
        COMMAND ${CMAKE_COMMAND} -E echo "Running repository shader normalizer..."
        COMMAND ${BASH_EXEC} "${REPO_NORMALIZER}" "${SHADERS_SRC_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "normalize_and_validate_shaders.sh ${SHADERS_SRC_DIR}"
        VERBATIM
      )
    endif()
  endif()
else()
  add_custom_target(exy_repo_normalize_shaders)
endif()

# ------------------------ Prepare shader files (pure CMake) -------------
# This section copies shader files to GENERATED_SHADER_DIR, ensures a default
# '#version 450' is present if not found, normalizes newlines, detects whether
# 'main' exists and then either compiles the shader or treats it as an include/header.
set(GENERATED_SHADER_C_FILES)

if(EXISTS "${SHADERS_SRC_DIR}")
  file(GLOB_RECURSE _ALL_SHADER_FILES
    "${SHADERS_SRC_DIR}/*.comp"
    "${SHADERS_SRC_DIR}/*.frag"
    "${SHADERS_SRC_DIR}/*.vert"
    "${SHADERS_SRC_DIR}/*.glsl"
    "${SHADERS_SRC_DIR}/*.spv"
  )

  foreach(_infile IN LISTS _ALL_SHADER_FILES)
    get_filename_component(_base ${_infile} NAME_WE)
    get_filename_component(_ext  ${_infile} EXT)
    string(TOLOWER "${_ext}" _ext_l)

    set(_sanitized "${GENERATED_SHADER_DIR}/${_base}${_ext_l}")
    set(_out_spv    "${GENERATED_SHADER_DIR}/${_base}.spv")
    set(_out_c      "${GENERATED_SHADER_DIR}/${_base}_spv.c")

    # Read original file into variable and normalize CRLF -> LF
    file(READ "${_infile}" _content_raw)
    string(REPLACE "\r\n" "\n" _content "${_content_raw}")
    string(REPLACE "\r" "\n" _content "${_content}")

    # NOTE: we do NOT attempt to match/strip a UTF-8 BOM via \x escapes here
    # because CMake's regex parser does not accept \x escapes. If your shader
    # files contain a BOM, either remove it locally or use the repo normalizer.
    #
    # Determine whether a '#version' directive is present as the first non-space token
    string(REGEX MATCH "^[[:space:]]*#version" _has_version "${_content}")
    if(_has_version)
      file(WRITE "${_sanitized}" "${_content}")
    else()
      # Prepend safe default '#version 450' for Vulkan shaders
      file(WRITE "${_sanitized}" "#version 450\n${_content}")
    endif()

    # Decide shader stage
    if(_ext_l STREQUAL ".comp")
      set(_stage "comp")
    elseif(_ext_l STREQUAL ".frag")
      set(_stage "frag")
    elseif(_ext_l STREQUAL ".vert")
      set(_stage "vert")
    else()
      set(_stage "comp")
    endif()

    # Detect whether sanitized file contains an entrypoint (simple regex)
    string(REGEX MATCH "void[[:space:]]+main[[:space:]]*\\(" _has_main "${_content}")
    if(NOT _has_main)
      string(REGEX MATCH "main[[:space:]]*\\(" _has_main "${_content}")
    endif()

    # Create compile-or-skip custom commands
    if(BUILD_SHADERS)
      if(_has_main)
        if(GLSLANG_VALIDATOR)
          add_custom_command(
            OUTPUT "${_out_spv}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling shader (glslangValidator): ${_sanitized}"
            COMMAND ${GLSLANG_VALIDATOR} -V --target-env ${SHADER_TARGET_ENV} -I "${GENERATED_SHADER_DIR}" -S ${_stage} -o "${_out_spv}" "${_sanitized}"
            DEPENDS "${_sanitized}"
            COMMENT "Compiling shader ${_sanitized} -> ${_out_spv}"
            VERBATIM
          )
        elseif(GLSLC_EXEC)
          add_custom_command(
            OUTPUT "${_out_spv}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling shader (glslc): ${_sanitized}"
            COMMAND ${GLSLC_EXEC} -fshader-stage=${_stage} --target-env=${SHADER_TARGET_ENV} -I "${GENERATED_SHADER_DIR}" "${_sanitized}" -o "${_out_spv}"
            DEPENDS "${_sanitized}"
            COMMENT "Compiling shader ${_sanitized} -> ${_out_spv}"
            VERBATIM
          )
        else()
          message(FATAL_ERROR "No shader compiler available.")
        endif()
      else()
        add_custom_command(
          OUTPUT "${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E echo "No entrypoint in ${_sanitized}: treat as include/header and touch ${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E touch "${_out_spv}"
          DEPENDS "${_sanitized}"
          COMMENT "Skipping compile for include ${_sanitized}"
          VERBATIM
        )
      endif()
    else()
      if(_ext_l STREQUAL ".spv")
        add_custom_command(
          OUTPUT "${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_infile}" "${_out_spv}"
          DEPENDS "${_infile}"
          COMMENT "Copy prebuilt SPV ${_infile}"
          VERBATIM
        )
      else()
        add_custom_command(
          OUTPUT "${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E touch "${_out_spv}"
          DEPENDS "${_sanitized}"
          COMMENT "Placeholder SPV (build disabled): ${_sanitized}"
          VERBATIM
        )
      endif()
    endif()

    # Embed SPIR-V into C using xxd -i
    add_custom_command(
      OUTPUT "${_out_c}"
      COMMAND ${CMAKE_COMMAND} -E echo "Embedding SPIR-V ${_out_spv} -> ${_out_c}"
      COMMAND ${XXD_EXEC} -i "${_out_spv}" "${_out_c}"
      DEPENDS "${_out_spv}"
      COMMENT "Embedding ${_out_spv} into ${_out_c}"
      VERBATIM
    )

    list(APPEND GENERATED_SHADER_C_FILES "${_out_c}")
  endforeach()
endif()

# Aggregate target
if(GENERATED_SHADER_C_FILES)
  add_custom_target(exy_generate_shaders DEPENDS ${GENERATED_SHADER_C_FILES})
  add_dependencies(exy_generate_shaders exy_repo_normalize_shaders)
else()
  add_custom_target(exy_generate_shaders)
endif()

# ------------------------ Project build --------------------------------
file(GLOB_RECURSE PROJECT_SRCS "${SRC_DIR}/*.c")
list(APPEND PROJECT_SRCS ${GENERATED_SHADER_C_FILES})

add_executable(exynostools ${PROJECT_SRCS})
add_dependencies(exynostools exy_generate_shaders)
target_include_directories(exynostools PRIVATE "${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

# ------------------------ Optimizations & link --------------------------
if(ENABLE_LTO AND NOT DEFINED ANDROID_NDK_HOME)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_out)
  if(ipo_supported)
    set_target_properties(exynostools PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endif()

if(NOT CMAKE_TOOLCHAIN_FILE)
  target_compile_options(exynostools PRIVATE -O3)
endif()
if(NOT MSVC)
  target_compile_options(exynostools PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
else()
  find_library(LIBVULKAN_NEEDED NAMES vulkan libvulkan vulkan-1 HINTS /usr/lib /usr/local/lib /lib /opt)
  if(LIBVULKAN_NEEDED)
    target_link_libraries(exynostools PRIVATE ${LIBVULKAN_NEEDED})
  else()
    message(WARNING "Vulkan not found; linking may fail if Vulkan symbols are required.")
  endif()
endif()

if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(exynostools PRIVATE ${CMAKE_THREAD_LIBS_INIT})
  find_library(LIBDL dl)
  if(LIBDL)
    target_link_libraries(exynostools PRIVATE ${LIBDL})
  endif()
endif()

set_target_properties(exynostools PROPERTIES OUTPUT_NAME "exynostools")
message(STATUS "CMake configured. BUILD_SHADERS=${BUILD_SHADERS} SHADER_TARGET_ENV=${SHADER_TARGET_ENV}")
