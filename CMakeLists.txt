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

    # Read original and normalize CRLF to LF
    file(READ "${_infile}" _raw_content)
    string(REPLACE "\r\n" "\n" _content "${_raw_content}")
    string(REPLACE "\r" "\n" _content "${_content}")

    # Ensure a sensible #version for Vulkan shaders if missing
    string(REGEX MATCH "^[[:space:]]*#version" _has_version "${_content}")
    if(_has_version)
      file(WRITE "${_sanitized}" "${_content}")
    else()
      file(WRITE "${_sanitized}" "#version 450\n${_content}")
    endif()

    # Determine shader stage (defaults to comp)
    if(_ext_l STREQUAL ".comp")
      set(_stage "comp")
    elseif(_ext_l STREQUAL ".frag")
      set(_stage "frag")
    elseif(_ext_l STREQUAL ".vert")
      set(_stage "vert")
    else()
      set(_stage "comp")
    endif()

    # Re-read sanitized content to detect main entrypoint
    file(READ "${_sanitized}" _san_content)
    string(REGEX MATCH "void[[:space:]]+main[[:space:]]*\\(" _has_main "${_san_content}")
    if(NOT _has_main)
      string(REGEX MATCH "main[[:space:]]*\\(" _has_main "${_san_content}")
    endif()

    # Create compile-or-touch rule
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

    # Embed the SPIR-V into C via xxd -i (create C array)
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

# Exclude selfcheck.c by default to avoid duplicate main unless BUILD_SELFTEST=ON
set(_selfcheck_path "${CMAKE_SOURCE_DIR}/src/selfcheck.c")
if(NOT BUILD_SELFTEST AND EXISTS "${_selfcheck_path}")
  message(STATUS "BUILD_SELFTEST=OFF: excluding ${_selfcheck_path} from build to avoid duplicate main")
  list(REMOVE_ITEM PROJECT_SRCS "${_selfcheck_path}")
endif()

# ---------------------- Xclipse driver shared lib -------------------------
set(XCLIPSE_SRCS "")
if(EXISTS "${DRIVERS_XCLIPSE_DIR}")
  file(GLOB_RECURSE XCLIPSE_SRCS "${DRIVERS_XCLIPSE_DIR}/*.c" "${DRIVERS_XCLIPSE_DIR}/*.h")
  # Filter out any accidentally included generated or top-level shared files that may conflict.
  # Exclude the common embed stub (if present) and any file names known to create duplicate symbols.
  list(FILTER XCLIPSE_SRCS EXCLUDE REGEX ".*shaders_embed_stub.*")
  list(FILTER XCLIPSE_SRCS EXCLUDE REGEX ".*shaders_embed.*")
  list(FILTER XCLIPSE_SRCS EXCLUDE REGEX ".*xeno_log_stream.*")
  list(FILTER XCLIPSE_SRCS EXCLUDE REGEX ".*xeno_wrapper_stubs.*")
endif()

if(XCLIPSE_SRCS)
  add_library(xclipse_driver SHARED ${XCLIPSE_SRCS})
  target_include_directories(xclipse_driver PRIVATE "${INCLUDE_DIR}" "${DRIVERS_XCLIPSE_DIR}")
  set_target_properties(xclipse_driver PROPERTIES OUTPUT_NAME "xclipse")
  if(NOT MSVC)
    target_compile_options(xclipse_driver PRIVATE -fPIC -fvisibility=hidden)
    # hide all symbols by default; exported API should be marked explicitly in driver headers if needed
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.13")
      set_target_properties(xclipse_driver PROPERTIES C_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON)
    endif()
  endif()

  # Link Vulkan for driver if available
  find_package(Vulkan QUIET)
  if(Vulkan_FOUND)
    target_include_directories(xclipse_driver PRIVATE ${Vulkan_INCLUDE_DIRS})
    target_link_libraries(xclipse_driver PRIVATE ${Vulkan_LIBRARIES})
  else()
    find_library(LIBVULKAN_NEEDED NAMES vulkan libvulkan vulkan-1 HINTS /usr/lib /usr/local/lib /lib /opt)
    if(LIBVULKAN_NEEDED)
      target_link_libraries(xclipse_driver PRIVATE ${LIBVULKAN_NEEDED})
    endif()
  endif()
endif()

# ---------------------- Build main executable -----------------------------
# Add generated shader C arrays to the binary so SPV symbols resolve
list(APPEND PROJECT_SRCS ${GENERATED_SHADER_C_FILES})

add_executable(exynostools ${PROJECT_SRCS})
add_dependencies(exynostools exy_generate_shaders)
target_include_directories(exynostools PRIVATE "${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

# IMPORTANT: do NOT link the shared driver into the executable to avoid duplicates.
# The driver is a plugin (.so) installed separately. If you need to load it at runtime,
# use dlopen() on the installed .so instead of linking against it.
# (This avoids multiple-definition errors when both the exe and plugin contain same symbols.)
#
# if(TARGET xclipse_driver)
#   target_link_libraries(exynostools PRIVATE xclipse_driver) <-- intentionally removed
# endif()

# ---------------------- Compiler / linker flags -------------------------
if(ENABLE_LTO AND NOT DEFINED ANDROID_NDK_HOME)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_supported OUTPUT ipo_out)
  if(ipo_supported)
    set_target_properties(exynostools PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    if(TARGET xclipse_driver)
      set_target_properties(xclipse_driver PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
  endif()
endif()

if(NOT CMAKE_TOOLCHAIN_FILE)
  target_compile_options(exynostools PRIVATE -O3)
  if(TARGET xclipse_driver)
    target_compile_options(xclipse_driver PRIVATE -O3)
  endif()
endif()

if(NOT MSVC)
  target_compile_options(exynostools PRIVATE -Wall -Wextra -Wno-unused-parameter)
  if(TARGET xclipse_driver)
    target_compile_options(xclipse_driver PRIVATE -Wall -Wextra -Wno-unused-parameter)
  endif()
endif()

# ---------------------- System libs -------------------------------------
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(exynostools PRIVATE ${CMAKE_THREAD_LIBS_INIT})
  find_library(LIBDL dl)
  if(LIBDL)
    target_link_libraries(exynostools PRIVATE ${LIBDL})
    if(TARGET xclipse_driver)
      target_link_libraries(xclipse_driver PRIVATE ${LIBDL})
    endif()
  endif()
endif()

# ---------------------- Install manifest (complete) ---------------------
install(TARGETS exynostools RUNTIME DESTINATION usr/bin)

if(TARGET xclipse_driver)
  install(TARGETS xclipse_driver LIBRARY DESTINATION usr/lib)
  install(DIRECTORY ${DRIVERS_XCLIPSE_DIR}/ DESTINATION usr/share/exynostools/drivers/xclipse FILES_MATCHING PATTERN "*.*")
endif()

if(EXISTS "${INCLUDE_DIR}")
  install(DIRECTORY ${INCLUDE_DIR}/ DESTINATION usr/include FILES_MATCHING PATTERN "*.h")
endif()

if(EXISTS "${GENERATED_SHADER_DIR}")
  install(DIRECTORY ${GENERATED_SHADER_DIR}/ DESTINATION usr/share/exynostools/shaders FILES_MATCHING PATTERN "*.spv" PATTERN "*_spv.c" PATTERN "*_spv.h")
endif()

if(EXISTS "${SHADERS_SRC_DIR}")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/shaders/ DESTINATION assets/shaders FILES_MATCHING PATTERN "*.comp" PATTERN "*.glsl" PATTERN "*.frag" PATTERN "*.vert")
endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/ DESTINATION usr/share/exynostools/src FILES_MATCHING PATTERN "*.c" PATTERN "*.h" PATTERN "*.inl" PATTERN "*.conf" PATTERN "*.json" PATTERN "*.xml" PATTERN "*.sh")

if(EXISTS "${CMAKE_SOURCE_DIR}/src/create_common_layouts_and_pipelines")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/create_common_layouts_and_pipelines/ DESTINATION usr/share/exynostools/layouts)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/src/app_profile")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/app_profile/ DESTINATION usr/share/exynostools/profiles)
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
  install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets/ DESTINATION assets)
endif()

message(STATUS "CMake configured. BUILD_SHADERS=${BUILD_SHADERS} BUILD_SELFTEST=${BUILD_SELFTEST} SHADER_TARGET_ENV=${SHADER_TARGET_ENV}")
