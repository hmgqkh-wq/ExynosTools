cmake_minimum_required(VERSION 3.15)
project(exynostools C)

# ---------------------------------------------------------------------------
# User options
# ---------------------------------------------------------------------------
option(BUILD_SHADERS "Compile GLSL shaders to SPIR-V and embed them into C sources" ON)
option(BUILD_SHARED_LIBS "Build as shared libraries where appropriate" OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Paths (adjust if your repo layout differs)
# ---------------------------------------------------------------------------
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(SHADERS_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SHADER_DIR "${CMAKE_BINARY_DIR}/generated_shaders")
file(MAKE_DIRECTORY "${GENERATED_SHADER_DIR}")

# Make sure generated dir is visible to include
include_directories("${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

# ---------------------------------------------------------------------------
# Find shader compilers/tools: prefer glslangValidator, then glslc
# ---------------------------------------------------------------------------
find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(GLSLC_EXEC glslc)
find_program(XXD_EXEC xxd)

# if BUILD_SHADERS=ON we need xxd and at least one compiler
if(BUILD_SHADERS)
  if(NOT XXD_EXEC)
    message(FATAL_ERROR "xxd not found (used to embed SPIR-V). Install xxd (vim-common) or provide pre-generated C embedding files and set BUILD_SHADERS=OFF.")
  endif()
  if(NOT GLSLANG_VALIDATOR AND NOT GLSLC_EXEC)
    message(FATAL_ERROR "No shader compiler found. Install glslangValidator or glslc (shaderc). Alternatively, set -DBUILD_SHADERS=OFF and provide prebuilt .spv files under assets/shaders.")
  endif()
endif()

# ---------------------------------------------------------------------------
# Collect shader sources and create generation rules
# - For each shader (glsl/comp/vert/frag) produce a .spv and then a _spv.c using xxd -i
# - Generated C files will produce <name>_spv and <name>_spv_len symbols.
# ---------------------------------------------------------------------------
set(GENERATED_SHADER_C_FILES)
if(EXISTS "${SHADERS_SRC_DIR}")
  file(GLOB_RECURSE _ALL_SHADER_FILES
    "${SHADERS_SRC_DIR}/*.comp"
    "${SHADERS_SRC_DIR}/*.comp.glsl"
    "${SHADERS_SRC_DIR}/*.frag"
    "${SHADERS_SRC_DIR}/*.vert"
    "${SHADERS_SRC_DIR}/*.glsl"
    "${SHADERS_SRC_DIR}/*.spv"
  )

  foreach(_shader IN LISTS _ALL_SHADER_FILES)
    get_filename_component(_base ${_shader} NAME_WE)   # e.g. bc1
    get_filename_component(_ext  ${_shader} EXT)      # e.g. .comp or .spv
    string(TOLOWER "${_ext}" _ext_l)
    set(_out_spv "${GENERATED_SHADER_DIR}/${_base}.spv")
    set(_out_c  "${GENERATED_SHADER_DIR}/${_base}_spv.c")

    if(BUILD_SHADERS)
      if(_ext_l STREQUAL ".spv")
        add_custom_command(
          OUTPUT "${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_shader}" "${_out_spv}"
          DEPENDS "${_shader}"
          COMMENT "Copy prebuilt SPIR-V ${_shader}"
          VERBATIM
        )
      else()
        if(GLSLANG_VALIDATOR)
          add_custom_command(
            OUTPUT "${_out_spv}"
            COMMAND ${GLSLANG_VALIDATOR} -V "${_shader}" -o "${_out_spv}"
            DEPENDS "${_shader}"
            COMMENT "Compiling shader (glslangValidator): ${_shader} -> ${_out_spv}"
            VERBATIM
          )
        elseif(GLSLC_EXEC)
          add_custom_command(
            OUTPUT "${_out_spv}"
            COMMAND ${GLSLC_EXEC} "${_shader}" -o "${_out_spv}"
            DEPENDS "${_shader}"
            COMMENT "Compiling shader (glslc): ${_shader} -> ${_out_spv}"
            VERBATIM
          )
        endif()
      endif()
    else()
      # BUILD_SHADERS=OFF: only accept prebuilt .spv sources
      if(NOT _ext_l STREQUAL ".spv")
        # skip non-spv sources when not building shaders
        continue()
      endif()
      add_custom_command(
        OUTPUT "${_out_spv}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_shader}" "${_out_spv}"
        DEPENDS "${_shader}"
        COMMENT "Copy prebuilt SPIR-V (build shaders disabled): ${_shader}"
        VERBATIM
      )
    endif()

    # Embed SPIR-V into C using xxd -i -> produces symbols like "<base>_spv" and "<base>_spv_len"
    add_custom_command(
      OUTPUT "${_out_c}"
      COMMAND ${CMAKE_COMMAND} -E echo "/* Auto-generated shader embed for ${_base} */" > "${_out_c}"
      COMMAND ${XXD_EXEC} -i "${_out_spv}" >> "${_out_c}"
      DEPENDS "${_out_spv}"
      COMMENT "Embedding SPIR-V ${_out_spv} -> ${_out_c}"
      VERBATIM
    )

    list(APPEND GENERATED_SHADER_C_FILES "${_out_c}")
  endforeach()
endif()

if(GENERATED_SHADER_C_FILES)
  add_custom_target(generate_shaders DEPENDS ${GENERATED_SHADER_C_FILES})
else()
  add_custom_target(generate_shaders)
endif()

# ---------------------------------------------------------------------------
# Project sources: collect all .c files under src/ (keeps full driver intact)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE PROJECT_SRCS "${SRC_DIR}/*.c")

# Add generated shader C files into the sources so they get compiled & linked
list(APPEND PROJECT_SRCS ${GENERATED_SHADER_C_FILES})

# ---------------------------------------------------------------------------
# Create main build target
# The original repository builds an executable/binary called exynostools.
# Keep that unchanged: build an executable target named exynostools.
# If you want a shared lib instead, change add_executable -> add_library(... SHARED)
# ---------------------------------------------------------------------------
add_executable(exynostools ${PROJECT_SRCS})
add_dependencies(exynostools generate_shaders)

# Include dirs (project headers + generated shader dir)
target_include_directories(exynostools PRIVATE "${INCLUDE_DIR}" "${GENERATED_SHADER_DIR}")

# Compiler warnings (safe defaults)
if(NOT MSVC)
  target_compile_options(exynostools PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Find Vulkan (try find_package, else try to link common system lib)
# ---------------------------------------------------------------------------
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  message(STATUS "Vulkan found: ${Vulkan_LIBRARY}")
  target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
else()
  find_library(LIBVULKAN_NEEDED NAMES vulkan libvulkan vulkan-1 HINTS /usr/lib /usr/local/lib /lib /opt)
  if(LIBVULKAN_NEEDED)
    message(STATUS "Linking with Vulkan library: ${LIBVULKAN_NEEDED}")
    target_link_libraries(exynostools PRIVATE ${LIBVULKAN_NEEDED})
  else()
    message(WARNING "Vulkan not found. Set Vulkan SDK paths or install libvulkan. Build may fail if Vulkan symbols are required.")
  endif()
endif()

# Link threading and dl on UNIX
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(exynostools PRIVATE ${CMAKE_THREAD_LIBS_INIT})
  find_library(LIBDL dl)
  if(LIBDL)
    target_link_libraries(exynostools PRIVATE ${LIBDL})
  endif()
endif()

set_target_properties(exynostools PROPERTIES OUTPUT_NAME "exynostools")

message(STATUS "Configured exynostools. BUILD_SHADERS=${BUILD_SHADERS}. Shader compiler: ${GLSLANG_VALIDATOR}${GLSLC_EXEC}")
