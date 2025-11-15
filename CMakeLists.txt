cmake_minimum_required(VERSION 3.15)
project(exynostools C)

# ----------------------------- Options -----------------------------------
option(BUILD_SHADERS "Compile GLSL shaders to SPIR-V and embed them into C sources" ON)
option(BUILD_SELFTEST "Build the selfcheck program (defines its own main). OFF prevents duplicate main." OFF)
option(ENABLE_LTO "Enable link-time optimization where supported" ON)
set(SHADER_TARGET_ENV "vulkan1.3" CACHE STRING "SPIR-V target environment (vulkan1.0|vulkan1.1|vulkan1.2|vulkan1.3)")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ----------------------------- Paths -------------------------------------
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(SHADERS_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SHADER_DIR "${CMAKE_BINARY_DIR}/generated_shaders")
file(MAKE_DIRECTORY "${GENERATED_SHADER_DIR}")

include_directories("${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

# ----------------------------- Tools -------------------------------------
find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(GLSLC_EXEC glslc)
find_program(XXD_EXEC xxd)
find_program(BASH_EXEC bash)   # optional: used only if your repo includes normalizer scripts

if(BUILD_SHADERS)
  if(NOT XXD_EXEC)
    message(FATAL_ERROR "Required tool 'xxd' not found. Install (e.g. vim-common) or set -DBUILD_SHADERS=OFF.")
  endif()
  if(NOT GLSLANG_VALIDATOR AND NOT GLSLC_EXEC)
    message(FATAL_ERROR "No shader compiler found (glslangValidator or glslc). Install one or set -DBUILD_SHADERS=OFF.")
  endif()
endif()

# ---------------------- Optional repo normalizer --------------------------
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

# ---------------------- Generate and compile shaders (pure CMake) ----------
# Behavior:
#  - copy shader sources to GENERATED_SHADER_DIR
#  - ensure '#version 450' at top if missing
#  - detect presence of 'main' via regex; compile only those files
#  - embed .spv -> .c using xxd -i
set(GENERATED_SHADER_C_FILES)

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

    # Read file and normalize CRLF -> LF; then write sanitized copy.
    file(READ "${_infile}" _raw_content)
    string(REPLACE "\r\n" "\n" _content "${_raw_content}")
    string(REPLACE "\r" "\n" _content "${_content}")

    # Check for '#version' directive as first non-space token.
    string(REGEX MATCH "^[[:space:]]*#version" _has_version "${_content}")
    if(_has_version)
      # Write sanitized copy
      file(WRITE "${_sanitized}" "${_content}")
    else()
      # Prepend '#version 450' safe default for Vulkan compute/fragment shaders.
      file(WRITE "${_sanitized}" "#version 450\n${_content}")
    endif()

    # Decide stage from extension (fallback to 'comp' for .glsl)
    if(_ext_l STREQUAL ".comp")
      set(_stage "comp")
    elseif(_ext_l STREQUAL ".frag")
      set(_stage "frag")
    elseif(_ext_l STREQUAL ".vert")
      set(_stage "vert")
    else()
      set(_stage "comp")
    endif()

    # Detect presence of entrypoint 'main' using CMake regex on the sanitized content
    file(READ "${_sanitized}" _san_content)  # re-read to pick up written content
    string(REGEX MATCH "void[[:space:]]+main[[:space:]]*\\(" _has_main "${_san_content}")
    if(NOT _has_main)
      string(REGEX MATCH "main[[:space:]]*\\(" _has_main "${_san_content}")
    endif()

    # Create compile or touch rule
    if(BUILD_SHADERS)
      if(_has_main)
        if(GLSLANG_VALIDATOR)
          add_custom_command(
            OUTPUT "${_out_spv}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${_sanitized} (glslangValidator)"
            COMMAND ${GLSLANG_VALIDATOR} -V --target-env ${SHADER_TARGET_ENV} -I "${GENERATED_SHADER_DIR}" -S ${_stage} -o "${_out_spv}" "${_sanitized}"
            DEPENDS "${_sanitized}"
            COMMENT "Compile ${_sanitized} -> ${_out_spv}"
            VERBATIM
          )
        else()
          add_custom_command(
            OUTPUT "${_out_spv}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${_sanitized} (glslc)"
            COMMAND ${GLSLC_EXEC} -fshader-stage=${_stage} --target-env=${SHADER_TARGET_ENV} -I "${GENERATED_SHADER_DIR}" "${_sanitized}" -o "${_out_spv}"
            DEPENDS "${_sanitized}"
            COMMENT "Compile ${_sanitized} -> ${_out_spv}"
            VERBATIM
          )
        endif()
      else()
        add_custom_command(
          OUTPUT "${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E echo "No entrypoint in ${_sanitized}: touching ${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E touch "${_out_spv}"
          DEPENDS "${_sanitized}"
          COMMENT "Skip compile for ${_sanitized} (include/header)"
          VERBATIM
        )
      endif()
    else()
      # BUILD_SHADERS OFF: copy .spv or touch placeholder
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
          COMMENT "Placeholder SPV (shaders disabled)"
          VERBATIM
        )
      endif()
    endif()

    # Embed SPIR-V into C via xxd -i (xxd required)
    add_custom_command(
      OUTPUT "${_out_c}"
      COMMAND ${CMAKE_COMMAND} -E echo "Embedding ${_out_spv} -> ${_out_c}"
      COMMAND ${XXD_EXEC} -i "${_out_spv}" "${_out_c}"
      DEPENDS "${_out_spv}"
      COMMENT "Embed ${_out_spv}"
      VERBATIM
    )

    list(APPEND GENERATED_SHADER_C_FILES "${_out_c}")
  endforeach()
endif()

# Aggregate generate target
if(GENERATED_SHADER_C_FILES)
  add_custom_target(exy_generate_shaders DEPENDS ${GENERATED_SHADER_C_FILES})
  add_dependencies(exy_generate_shaders exy_repo_normalize_shaders)
else()
  add_custom_target(exy_generate_shaders)
endif()

# ---------------------- Project source gather (conditionally exclude selfcheck) ----------
file(GLOB_RECURSE PROJECT_SRCS "${SRC_DIR}/*.c")

# Exclude selfcheck.c by default to avoid duplicate main unless BUILD_SELFTEST enabled
set(_selfcheck_path "${CMAKE_SOURCE_DIR}/src/selfcheck.c")
if(NOT BUILD_SELFTEST AND EXISTS "${_selfcheck_path}")
  message(STATUS "BUILD_SELFTEST=OFF: excluding ${_selfcheck_path} from build to avoid duplicate main")
  list(REMOVE_ITEM PROJECT_SRCS "${_selfcheck_path}")
endif()

# Append generated shader C files so symbols resolve
list(APPEND PROJECT_SRCS ${GENERATED_SHADER_C_FILES})

# Create binary
add_executable(exynostools ${PROJECT_SRCS})
add_dependencies(exynostools exy_generate_shaders)
target_include_directories(exynostools PRIVATE "${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

# ---------------------- Compiler / linker flags -------------------------
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

# ---------------------- Find Vulkan and link libs -----------------------
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

# ---------------------- Install manifest (complete) ---------------------
install(TARGETS exynostools RUNTIME DESTINATION usr/bin)

if(EXISTS "${CMAKE_SOURCE_DIR}/include")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION usr/include FILES_MATCHING PATTERN "*.h")
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/ DESTINATION usr/share/exynostools/src
        FILES_MATCHING
        PATTERN "*.c"
        PATTERN "*.h"
        PATTERN "*.inl"
        PATTERN "*.inc")

if(EXISTS "${CMAKE_SOURCE_DIR}/src/drivers/xclipse")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/drivers/xclipse/ DESTINATION usr/share/exynostools/drivers/xclipse FILES_MATCHING PATTERN "*.*")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/drivers/async")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/drivers/async/ DESTINATION usr/share/exynostools/drivers/async FILES_MATCHING PATTERN "*.*")
endif()

if(EXISTS "${CMAKE_BINARY_DIR}/generated_shaders")
  install(DIRECTORY ${CMAKE_BINARY_DIR}/generated_shaders/ DESTINATION usr/share/exynostools/shaders FILES_MATCHING PATTERN "*.spv" PATTERN "*.c" PATTERN "*.h")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/assets/shaders")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/shaders/ DESTINATION assets/shaders FILES_MATCHING PATTERN "*.comp" PATTERN "*.glsl" PATTERN "*.frag" PATTERN "*.vert")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/ DESTINATION assets)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/create_common_layouts_and_pipelines")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/create_common_layouts_and_pipelines/ DESTINATION usr/share/exynostools/layouts)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/src/app_profile")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/app_profile/ DESTINATION usr/share/exynostools/profiles)
endif()

message(STATUS "CMake configured. BUILD_SHADERS=${BUILD_SHADERS} BUILD_SELFTEST=${BUILD_SELFTEST} SHADER_TARGET_ENV=${SHADER_TARGET_ENV}")
