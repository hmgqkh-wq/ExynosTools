cmake_minimum_required(VERSION 3.15)
project(exynostools C)

# ----------------------------- Options -----------------------------------
option(BUILD_SHADERS "Compile GLSL shaders to SPIR-V and embed them into C sources" ON)
option(ENABLE_LTO "Enable link-time optimization where supported" ON)
set(SHADER_TARGET_ENV "vulkan1.3" CACHE STRING "SPIR-V target environment")
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
find_program(BASH_EXEC bash)

if(BUILD_SHADERS)
  if(NOT XXD_EXEC)
    message(FATAL_ERROR "xxd not found. Install xxd (vim-common) or set BUILD_SHADERS=OFF and supply prebuilt .spv files.")
  endif()
  if(NOT GLSLANG_VALIDATOR AND NOT GLSLC_EXEC)
    message(FATAL_ERROR "No shader compiler found. Install glslangValidator or glslc (shaderc), or set BUILD_SHADERS=OFF.")
  endif()
  if(NOT BASH_EXEC)
    message(FATAL_ERROR "bash not found; required to run repo shader scripts.")
  endif()
endif()

# ---------------------- Repo normalizer & prep (safe names) ----------------
set(REPO_NORMALIZER "${CMAKE_SOURCE_DIR}/normalize_and_validate_shaders.sh")
if(BUILD_SHADERS AND EXISTS "${REPO_NORMALIZER}")
  if(NOT TARGET exy_repo_normalize_shaders)
    add_custom_target(exy_repo_normalize_shaders
      COMMAND ${CMAKE_COMMAND} -E echo "Running repo shader normalizer..."
      COMMAND ${BASH_EXEC} "${REPO_NORMALIZER}" "${SHADERS_SRC_DIR}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "normalize_and_validate_shaders.sh ${SHADERS_SRC_DIR}"
      VERBATIM
    )
  endif()
else()
  add_custom_target(exy_repo_normalize_shaders)
endif()

set(PREP_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/prep_shaders.sh")
if(BUILD_SHADERS AND EXISTS "${PREP_SCRIPT}")
  if(NOT TARGET exy_repo_prep_shaders)
    add_custom_target(exy_repo_prep_shaders
      COMMAND ${CMAKE_COMMAND} -E echo "Running local shader preprocessor..."
      COMMAND ${BASH_EXEC} "${PREP_SCRIPT}" "${SHADERS_SRC_DIR}" "${GENERATED_SHADER_DIR}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "prep_shaders.sh ${SHADERS_SRC_DIR}"
      VERBATIM
    )
  endif()
else()
  add_custom_target(exy_repo_prep_shaders)
endif()

# ---------------------- Build shader list (compile only entrypoints) --------
set(GENERATED_SHADER_C_FILES)
if(EXISTS "${SHADERS_SRC_DIR}")
  # First, ensure normalization/prep runs before generation
  file(GLOB_RECURSE _ALL_SRC_FILES
    "${SHADERS_SRC_DIR}/*.comp"
    "${SHADERS_SRC_DIR}/*.frag"
    "${SHADERS_SRC_DIR}/*.vert"
    "${SHADERS_SRC_DIR}/*.glsl"
    "${SHADERS_SRC_DIR}/*.spv"
  )

  foreach(_infile IN LISTS _ALL_SRC_FILES)
    get_filename_component(_base ${_infile} NAME_WE)
    get_filename_component(_ext  ${_infile} EXT)
    string(TOLOWER "${_ext}" _ext_l)

    # We will compile from the generated/sanitized directory when BUILD_SHADERS is on
    set(_sanitized "${GENERATED_SHADER_DIR}/${base}${_ext}")
    set(_out_spv "${GENERATED_SHADER_DIR}/${_base}.spv")
    set(_out_c  "${GENERATED_SHADER_DIR}/${_base}_spv.c")

    if(BUILD_SHADERS)
      # ensure sanitized copy exists: prep_shaders.sh will handle glsl->generated copies.
      # If SKIP prep, we still copy original to generated to avoid editing originals.
      add_custom_command(OUTPUT "${_sanitized}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_infile}" "${_sanitized}"
        DEPENDS "${_infile}"
        COMMENT "Copy source to generated shader dir: ${_infile}"
        VERBATIM
      )
    else()
      # if not generating shaders, compile from original files (or copy .spv)
      set(_sanitized "${_infile}")
    endif()

    # Decide whether the file is an entrypoint (compile) or a header (skip compile)
    # We detect entrypoints by looking for 'void main' or 'main(' within the sanitized file.
    # Use a custom command that will at configure-time read the file contents to decide.
    # For portability, we'll create a rule that compiles only if file contains main; otherwise just copy.
    add_custom_command(OUTPUT "${_out_spv}"
      COMMAND ${CMAKE_COMMAND} -E echo "Checking shader entrypoint for ${_sanitized}"
      # If the sanitized file contains 'void main' or 'main(', attempt compile; else just copy (no compile)
      COMMAND ${CMAKE_COMMAND} -E sh -c "
        if grep -qE 'void[[:space:]]+main[[:space:]]*\\(|main[[:space:]]*\\(' '${_sanitized}'; then
          echo 'Entrypoint detected: compiling ${_sanitized}';
          if [ -n \"${GLSLANG_VALIDATOR}\" ]; then
            ${GLSLANG_VALIDATOR} -V --target-env ${SHADER_TARGET_ENV} -I '${GENERATED_SHADER_DIR}' -S ${_ext_l:1} -o '${_out_spv}' '${_sanitized}';
          else
            ${GLSLC_EXEC} -fshader-stage=${_ext_l:1} --target-env=${SHADER_TARGET_ENV} -I '${GENERATED_SHADER_DIR}' '${_sanitized}' -o '${_out_spv}';
          fi
        else
          echo 'No entrypoint found in ${_sanitized}: skipping compile (treat as include/header)';
          ${CMAKE_COMMAND} -E touch '${_out_spv}';
        fi
      "
      DEPENDS "${_sanitized}"
      COMMENT "Compile or skip shader ${_sanitized}"
      VERBATIM
    )

    # Only embed when SPIR-V actually exists (touch may create placeholder)
    add_custom_command(OUTPUT "${_out_c}"
      COMMAND ${CMAKE_COMMAND} -E echo "Embedding SPIR-V ${_out_spv} -> ${_out_c}"
      COMMAND ${CMAKE_COMMAND} -E sh -c "if [ -s '${_out_spv}' ]; then ${XXD_EXEC} -i '${_out_spv}' > '${_out_c}'; else echo '/* no spv generated for ${_base} */' > '${_out_c}'; fi"
      DEPENDS "${_out_spv}"
      COMMENT "Embedding ${_out_spv} -> ${_out_c}"
      VERBATIM
    )

    list(APPEND GENERATED_SHADER_C_FILES "${_out_c}")
  endforeach()
endif()

# Ensure generate_shaders target exists and depends on normalization + prep
if(GENERATED_SHADER_C_FILES)
  add_custom_target(exy_generate_shaders DEPENDS ${GENERATED_SHADER_C_FILES})
  add_dependencies(exy_generate_shaders exy_repo_normalize_shaders)
  add_dependencies(exy_generate_shaders exy_repo_prep_shaders)
else()
  add_custom_target(exy_generate_shaders)
endif()

# ---------------------- Project sources (unchanged) ----------------------
file(GLOB_RECURSE PROJECT_SRCS "${SRC_DIR}/*.c")
list(APPEND PROJECT_SRCS ${GENERATED_SHADER_C_FILES})

add_executable(exynostools ${PROJECT_SRCS})
add_dependencies(exynostools exy_generate_shaders)

target_include_directories(exynostools PRIVATE "${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

# ---------------------- Build optimization flags -------------------------
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

# ---------------------- Find Vulkan and system libs ----------------------
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
