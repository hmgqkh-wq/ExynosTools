cmake_minimum_required(VERSION 3.15)
project(exynostools C)

# -------------------- User options --------------------
option(BUILD_SHADERS "Compile GLSL shaders to SPIR-V and embed them into C sources" ON)
option(ENABLE_LTO "Enable link-time optimization where supported" ON)
set(SHADER_TARGET_ENV "vulkan1.3" CACHE STRING "SPIR-V target environment (vulkan1.0, vulkan1.1, vulkan1.2, vulkan1.3)")
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# -------------------- Paths --------------------
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(SHADERS_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SHADER_DIR "${CMAKE_BINARY_DIR}/generated_shaders")
file(MAKE_DIRECTORY "${GENERATED_SHADER_DIR}")

include_directories("${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

# -------------------- Tools --------------------
find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(GLSLC_EXEC glslc)
find_program(XXD_EXEC xxd)
find_program(BASH_EXEC bash) # used to run repo scripts even if not executable

if(BUILD_SHADERS)
  if(NOT XXD_EXEC)
    message(FATAL_ERROR "xxd not found (used to embed SPIR-V). Install xxd (vim-common) or set BUILD_SHADERS=OFF and provide prebuilt .spv files.")
  endif()
  if(NOT GLSLANG_VALIDATOR AND NOT GLSLC_EXEC)
    message(FATAL_ERROR "No shader compiler found (glslangValidator or glslc). Install one or set BUILD_SHADERS=OFF.")
  endif()
endif()

# -------------------- Repo normalizer (if present) --------------------
# Use unique target names to avoid collision with any existing repository targets.
set(REPO_NORMALIZER_SCRIPT "${CMAKE_SOURCE_DIR}/normalize_and_validate_shaders.sh")
if(BUILD_SHADERS AND EXISTS "${REPO_NORMALIZER_SCRIPT}")
  # Use bash explicitly so script need not be executable in the repo.
  if(NOT BASH_EXEC)
    message(FATAL_ERROR "bash not found on system; required to run ${REPO_NORMALIZER_SCRIPT}")
  endif()

  # Only create our custom target if it does not already exist in the project.
  if(NOT TARGET exy_repo_normalize_shaders)
    add_custom_target(exy_repo_normalize_shaders
      COMMAND ${CMAKE_COMMAND} -E echo "Running repository shader normalizer..."
      COMMAND ${BASH_EXEC} "${REPO_NORMALIZER_SCRIPT}" "${SHADERS_SRC_DIR}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "normalize_and_validate_shaders.sh ${SHADERS_SRC_DIR}"
      VERBATIM
    )
  else()
    message(STATUS "Target exy_repo_normalize_shaders already exists; skipping creation.")
  endif()
else()
  add_custom_target(exy_repo_normalize_shaders)
endif()

# -------------------- Local shader preprocessing (optional) ------------
set(PREP_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/prep_shaders.sh")
if(BUILD_SHADERS AND EXISTS "${PREP_SCRIPT}")
  if(NOT BASH_EXEC)
    message(FATAL_ERROR "bash not found on system; required to run ${PREP_SCRIPT}")
  endif()

  if(NOT TARGET exy_repo_prep_shaders)
    add_custom_target(exy_repo_prep_shaders
      COMMAND ${CMAKE_COMMAND} -E echo "Running local shader preprocessor..."
      COMMAND ${BASH_EXEC} "${PREP_SCRIPT}" "${SHADERS_SRC_DIR}" "${GENERATED_SHADER_DIR}"
      WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
      COMMENT "prep_shaders.sh ${SHADERS_SRC_DIR}"
      VERBATIM
    )
  else()
    message(STATUS "Target exy_repo_prep_shaders already exists; skipping creation.")
  endif()
else()
  add_custom_target(exy_repo_prep_shaders)
endif()

# -------------------- Generate SPIR-V and embed into C -------------------
set(GENERATED_SHADER_C_FILES)
if(EXISTS "${SHADERS_SRC_DIR}")
  file(GLOB_RECURSE _ALL_SHADER_FILES
    "${SHADERS_SRC_DIR}/*.comp"
    "${SHADERS_SRC_DIR}/*.frag"
    "${SHADERS_SRC_DIR}/*.vert"
    "${SHADERS_SRC_DIR}/*.glsl"
    "${SHADERS_SRC_DIR}/*.spv"
  )

  foreach(_shader IN LISTS _ALL_SHADER_FILES)
    get_filename_component(_base ${_shader} NAME_WE)
    get_filename_component(_ext  ${_shader} EXT)
    string(TOLOWER "${_ext}" _ext_l)
    set(_out_spv "${GENERATED_SHADER_DIR}/${_base}.spv")
    set(_out_c  "${GENERATED_SHADER_DIR}/${_base}_spv.c")

    if(BUILD_SHADERS)
      if(_ext_l STREQUAL ".spv")
        add_custom_command(OUTPUT "${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_shader}" "${_out_spv}"
          DEPENDS "${_shader}"
          COMMENT "Copy prebuilt SPIR-V ${_shader}"
          VERBATIM)
      else()
        if(_ext_l STREQUAL ".comp")
          set(_stage "comp")
        elseif(_ext_l STREQUAL ".frag")
          set(_stage "frag")
        elseif(_ext_l STREQUAL ".vert")
          set(_stage "vert")
        else()
          set(_stage "comp")
        endif()

        if(GLSLANG_VALIDATOR)
          add_custom_command(OUTPUT "${_out_spv}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling shader (glslangValidator) ${_shader}"
            COMMAND ${GLSLANG_VALIDATOR} -V --target-env ${SHADER_TARGET_ENV} -S ${_stage} -o "${_out_spv}" "${_shader}"
            DEPENDS "${_shader}"
            COMMENT "Compiling shader with glslangValidator: ${_shader}"
            VERBATIM)
        elseif(GLSLC_EXEC)
          add_custom_command(OUTPUT "${_out_spv}"
            COMMAND ${CMAKE_COMMAND} -E echo "Compiling shader (glslc) ${_shader}"
            COMMAND ${GLSLC_EXEC} -fshader-stage=${_stage} --target-env=${SHADER_TARGET_ENV} "${_shader}" -o "${_out_spv}"
            DEPENDS "${_shader}"
            COMMENT "Compiling shader with glslc: ${_shader}"
            VERBATIM)
        endif()
      endif()
    else()
      if(NOT _ext_l STREQUAL ".spv")
        continue()
      endif()
      add_custom_command(OUTPUT "${_out_spv}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_shader}" "${_out_spv}"
        DEPENDS "${_shader}"
        COMMENT "Copy prebuilt SPIR-V (build disabled): ${_shader}"
        VERBATIM)
    endif()

    add_custom_command(OUTPUT "${_out_c}"
      COMMAND ${CMAKE_COMMAND} -E echo "/* Auto-generated shader embed for ${_base} */" > "${_out_c}"
      COMMAND ${XXD_EXEC} -i "${_out_spv}" >> "${_out_c}"
      DEPENDS "${_out_spv}"
      COMMENT "Embedding SPIR-V ${_out_spv} -> ${_out_c}"
      VERBATIM)

    list(APPEND GENERATED_SHADER_C_FILES "${_out_c}")
  endforeach()
endif()

if(GENERATED_SHADER_C_FILES)
  add_custom_target(exy_generate_shaders DEPENDS ${GENERATED_SHADER_C_FILES})
  add_dependencies(exy_generate_shaders exy_repo_normalize_shaders)
  add_dependencies(exy_generate_shaders exy_repo_prep_shaders)
else()
  add_custom_target(exy_generate_shaders)
endif()

# -------------------- Gather project sources (no changes to driver) ----
file(GLOB_RECURSE PROJECT_SRCS "${SRC_DIR}/*.c")
list(APPEND PROJECT_SRCS ${GENERATED_SHADER_C_FILES})

add_executable(exynostools ${PROJECT_SRCS})
add_dependencies(exynostools exy_generate_shaders)

target_include_directories(exynostools PRIVATE "${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

# -------------------- Optimizations (safe defaults) --------------------
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

# -------------------- Find Vulkan and system libs -----------------------
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
