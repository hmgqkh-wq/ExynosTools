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

# Output paths (keep .so in build root for your script)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# ----------------------------- Paths -------------------------------------
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(DRIVERS_XCLIPSE_DIR "${CMAKE_SOURCE_DIR}/src/drivers/xclipse")
set(SHADERS_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SHADER_DIR "${CMAKE_BINARY_DIR}/generated_shaders")
file(MAKE_DIRECTORY "${GENERATED_SHADER_DIR}")

set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
include_directories("${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

# ----------------------------- Tools -------------------------------------
find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(GLSLC_EXEC glslc)
find_program(XXD_EXEC xxd)

if(BUILD_SHADERS)
  if(NOT XXD_EXEC)
    message(FATAL_ERROR "Required tool 'xxd' not found. Install (e.g. vim-common) or set -DBUILD_SHADERS=OFF.")
  endif()
  if(NOT GLSLANG_VALIDATOR AND NOT GLSLC_EXEC)
    message(FATAL_ERROR "No shader compiler found (glslangValidator or glslc). Install one or set -DBUILD_SHADERS=OFF.")
  endif()
endif()

# ---------------------- Generate and compile shaders (pure CMake) ----------
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

    # Normalize line endings and ensure #version
    file(READ "${_infile}" _raw_content)
    string(REPLACE "\r\n" "\n" _content "${_raw_content}")
    string(REPLACE "\r" "\n" _content "${_content}")
    string(REGEX MATCH "^[[:space:]]*#version" _has_version "${_content}")
    if(_has_version)
      file(WRITE "${_sanitized}" "${_content}")
    else()
      file(WRITE "${_sanitized}" "#version 450\n${_content}")
    endif()

    # Determine stage
    if(_ext_l STREQUAL ".comp")
      set(_stage "comp")
    elseif(_ext_l STREQUAL ".frag")
      set(_stage "frag")
    elseif(_ext_l STREQUAL ".vert")
      set(_stage "vert")
    else()
      set(_stage "comp")
    endif()

    # Detect entrypoint
    file(READ "${_sanitized}" _san_content)
    string(REGEX MATCH "void[[:space:]]+main[[:space:]]*\\(" _has_main "${_san_content}")
    if(NOT _has_main)
      string(REGEX MATCH "main[[:space:]]*\\(" _has_main "${_san_content}")
    endif()

    # Compile or touch
    if(BUILD_SHADERS)
      if(_has_main)
        if(GLSLANG_VALIDATOR)
          add_custom_command(
            OUTPUT "${_out_spv}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${_sanitized} -> ${_out_spv} (glslangValidator)"
            COMMAND ${GLSLANG_VALIDATOR} -V --target-env ${SHADER_TARGET_ENV} -I "${GENERATED_SHADER_DIR}" -S ${_stage} -o "${_out_spv}" "${_sanitized}"
            DEPENDS "${_sanitized}"
            COMMENT "Compile shader ${_sanitized}"
            VERBATIM
          )
        else()
          add_custom_command(
            OUTPUT "${_out_spv}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling ${_sanitized} -> ${_out_spv} (glslc)"
            COMMAND ${GLSLC_EXEC} -fshader-stage=${_stage} --target-env=${SHADER_TARGET_ENV} -I "${GENERATED_SHADER_DIR}" "${_sanitized}" -o "${_out_spv}"
            DEPENDS "${_sanitized}"
            COMMENT "Compile shader ${_sanitized}"
            VERBATIM
          )
        endif()
      else()
        add_custom_command(
          OUTPUT "${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E echo "No entrypoint in ${_sanitized}: creating placeholder ${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E touch "${_out_spv}"
          DEPENDS "${_sanitized}"
          COMMENT "Skip compile for include ${_sanitized}"
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
          COMMENT "Placeholder SPV (shaders disabled)"
          VERBATIM
        )
      endif()
    endif()

    # Embed SPIR-V into C via xxd -i
    add_custom_command(
      OUTPUT "${_out_c}"
      COMMAND ${CMAKE_COMMAND} -E echo "Embedding ${_out_spv} -> ${_out_c}"
      COMMAND ${XXD_EXEC} -i "${_out_spv}" "${_out_c}"
      DEPENDS "${_out_spv}"
      COMMENT "Embed SPV to C ${_out_spv}"
      VERBATIM
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

# ---------------------- Collect project sources --------------------------
file(GLOB_RECURSE PROJECT_SRCS "${SRC_DIR}/*.c")

# Exclude selfcheck.c unless requested
set(_selfcheck_path "${CMAKE_SOURCE_DIR}/src/selfcheck.c")
if(NOT BUILD_SELFTEST AND EXISTS "${_selfcheck_path}")
  list(REMOVE_ITEM PROJECT_SRCS "${_selfcheck_path}")
endif()

# Exclude known conflicts from the host executable
set(_conflict_files
  "${CMAKE_SOURCE_DIR}/src/shaders_embed_stub.c"
  "${CMAKE_SOURCE_DIR}/src/shaders_embed.c"
  "${CMAKE_SOURCE_DIR}/src/xeno_log_stream.c"
  "${CMAKE_SOURCE_DIR}/src/xeno_log_stream_stub.c"
  "${CMAKE_SOURCE_DIR}/src/xeno_wrapper_stubs.c"
)
foreach(_cf IN LISTS _conflict_files)
  if(EXISTS "${_cf}")
    list(REMOVE_ITEM PROJECT_SRCS "${_cf}")
  endif()
endforeach()

# ---------------------- Xeno wrapper shared lib (.so plugin) --------------
# Combine driver sources + wrapper; include generated shader arrays
set(XCLIPSE_SRCS "")
if(EXISTS "${DRIVERS_XCLIPSE_DIR}")
  file(GLOB_RECURSE XCLIPSE_SRCS "${DRIVERS_XCLIPSE_DIR}/*.c" "${DRIVERS_XCLIPSE_DIR}/*.h")
endif()

# Wrapper sources (adjust if you rename files)
set(WRAPPER_SRCS "")
foreach(f
  xeno_wrapper.c
  xclipse_wrapper.c
  rt_path.c
  async.c
  vrs.c
  emulate.c
  bc_emulate.c
  create_common_layouts_and_pipelines.c
)
  if(EXISTS "${SRC_DIR}/${f}")
    list(APPEND WRAPPER_SRCS "${SRC_DIR}/${f}")
  endif()
endforeach()

add_library(xeno_wrapper SHARED ${XCLIPSE_SRCS} ${WRAPPER_SRCS})
add_dependencies(xeno_wrapper exy_generate_shaders)
target_include_directories(xeno_wrapper PRIVATE "${INCLUDE_DIR}" "${DRIVERS_XCLIPSE_DIR}" "${GENERATED_SHADER_DIR}")

if(GENERATED_SHADER_C_FILES)
  target_sources(xeno_wrapper PRIVATE ${GENERATED_SHADER_C_FILES})
endif()

# Output name must be libxeno_wrapper.so at build root
set_target_properties(xeno_wrapper PROPERTIES
  OUTPUT_NAME "xeno_wrapper"
  C_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)

if(NOT MSVC)
  target_compile_options(xeno_wrapper PRIVATE -fPIC -fvisibility=hidden -O3 -Wall -Wextra -Wno-unused-parameter)
endif()

# Link Vulkan
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  target_include_directories(xeno_wrapper PRIVATE ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(xeno_wrapper PRIVATE ${Vulkan_LIBRARIES})
else()
  find_library(LIBVULKAN_NEEDED NAMES vulkan libvulkan vulkan-1 HINTS /usr/lib /usr/local/lib /lib /opt)
  if(LIBVULKAN_NEEDED)
    target_link_libraries(xeno_wrapper PRIVATE ${LIBVULKAN_NEEDED})
  endif()
endif()

# Optional: export init symbol hint (ensure your source defines xeno_init)
target_compile_definitions(xeno_wrapper PRIVATE XENO_EXPORT_INIT_SYMBOL=xeno_init)

# ---------------------- Host executable (optional) ------------------------
add_executable(exynostools ${PROJECT_SRCS})
add_dependencies(exynostools exy_generate_shaders)
target_include_directories(exynostools PRIVATE "${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")
if(NOT MSVC)
  target_compile_options(exynostools PRIVATE -O3 -Wall -Wextra -Wno-unused-parameter)
endif()

# System libs
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(exynostools PRIVATE ${CMAKE_THREAD_LIBS_INIT})
  find_library(LIBDL dl)
  if(LIBDL)
    target_link_libraries(exynostools PRIVATE ${LIBDL})
    target_link_libraries(xeno_wrapper PRIVATE ${LIBDL})
  endif()
endif()

# LTO
if(ENABLE_LTO AND NOT DEFINED ANDROID_NDK_HOME)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_out)
  if(ipo_supported)
    set_target_properties(exynostools PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    set_target_properties(xeno_wrapper PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
  endif()
endif()

# ---------------------- Install to staging (Winlator-style) ---------------
# Install plugin
install(TARGETS xeno_wrapper LIBRARY DESTINATION usr/lib)
# Install host exe (optional)
install(TARGETS exynostools RUNTIME DESTINATION usr/bin)

# Install generated shaders (SPV + embedded C)
if(EXISTS "${GENERATED_SHADER_DIR}")
  install(DIRECTORY ${GENERATED_SHADER_DIR}/ DESTINATION usr/share/exynostools/shaders
          FILES_MATCHING PATTERN "*.spv" PATTERN "*_spv.c" PATTERN "*_spv.h")
endif()

# Install raw shader sources for debugging
if(EXISTS "${SHADERS_SRC_DIR}")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/shaders/src/ DESTINATION usr/share/exynostools/shaders/src
          FILES_MATCHING PATTERN "*.comp" PATTERN "*.glsl" PATTERN "*.frag" PATTERN "*.vert")
endif()

# Install headers (after you removed duplicates)
if(EXISTS "${INCLUDE_DIR}")
  install(DIRECTORY ${INCLUDE_DIR}/ DESTINATION usr/include FILES_MATCHING PATTERN "*.h")
endif()

# Install any runtime configs you keep in repo (script will copy final ones from etc/)
if(EXISTS "${CMAKE_SOURCE_DIR}/src")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/ DESTINATION usr/share/exynostools/src
          FILES_MATCHING PATTERN "*.c" PATTERN "*.h" PATTERN "*.inl" PATTERN "*.conf" PATTERN "*.json" PATTERN "*.xml" PATTERN "*.sh")
endif()

message(STATUS "Configured. BUILD_SHADERS=${BUILD_SHADERS} BUILD_SELFTEST=${BUILD_SELFTEST} SHADER_TARGET_ENV=${SHADER_TARGET_ENV}")
