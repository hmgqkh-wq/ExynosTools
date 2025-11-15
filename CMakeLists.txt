cmake_minimum_required(VERSION 3.15)
project(exynostools C)

# --- Basic options ---
option(BUILD_SHADERS "Compile GLSL shaders to SPIR-V and embed them into C" ON)
option(BUILD_SHARED_LIBS "Build as shared libraries where appropriate" OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Helpful variables ---
set(SHADERS_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
set(GENERATED_SHADER_DIR "${CMAKE_BINARY_DIR}/generated_shaders")
file(MAKE_DIRECTORY "${GENERATED_SHADER_DIR}")
include_directories("${CMAKE_SOURCE_DIR}/include" "${GENERATED_SHADER_DIR}")

# --- Find tools: glslc and xxd (xxd is used to embed the .spv binary into C-arrays) ---
find_program(GLSLC_EXEC glslc)
find_program(XXD_EXEC xxd)

if(BUILD_SHADERS)
  if(NOT XXD_EXEC)
    message(FATAL_ERROR "xxd not found (needed to embed SPIR-V into C). Install xxd or set BUILD_SHADERS=OFF.")
  endif()
endif()

# --- Collect shader source files (GLSL/SPV). We'll produce a .c embed per shader. ---
set(_shader_sources "")
if(EXISTS "${SHADERS_DIR}")
  file(GLOB_RECURSE _glsl_files
    "${SHADERS_DIR}/*.comp"
    "${SHADERS_DIR}/*.frag"
    "${SHADERS_DIR}/*.vert"
    "${SHADERS_DIR}/*.glsl"
    "${SHADERS_DIR}/*.spv"
    "${SHADERS_DIR}/*.spvasm"
  )

  foreach(_infile IN LISTS _glsl_files)
    get_filename_component(_inname ${_infile} NAME_WE)       # base name
    get_filename_component(_inext ${_infile} EXT)           # extension with dot
    string(REPLACE "." "" _inext_no_dot "${_inext}")

    # normalized base filename for generated files
    set(_out_spv "${GENERATED_SHADER_DIR}/${_inname}.spv")
    set(_out_c  "${GENERATED_SHADER_DIR}/${_inname}_spv.c")

    if(BUILD_SHADERS)
      # If the file is already a .spv, we just copy; otherwise try to compile with glslc
      string(TOLOWER "${_inext_no_dot}" _ext_lower)
      if("${_ext_lower}" STREQUAL "spv")
        add_custom_command(
          OUTPUT "${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_infile}" "${_out_spv}"
          DEPENDS "${_infile}"
          COMMENT "Copy existing SPIR-V: ${_infile} -> ${_out_spv}"
          VERBATIM
        )
      else()
        if(NOT GLSLC_EXEC)
          message(FATAL_ERROR "glslc not found but required to compile shader ${_infile}. Install shaderc/glslc or set BUILD_SHADERS=OFF and provide prebuilt .spv files.")
        endif()
        add_custom_command(
          OUTPUT "${_out_spv}"
          COMMAND ${GLSLC_EXEC} "${_infile}" -o "${_out_spv}"
          DEPENDS "${_infile}"
          WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
          COMMENT "Compiling shader ${_infile} -> ${_out_spv}"
          VERBATIM
        )
      endif()
    else()
      # If BUILD_SHADERS=OFF: only consider prebuilt .spv files in the shaders dir
      if("${_ext_lower}" STREQUAL "spv")
        add_custom_command(
          OUTPUT "${_out_spv}"
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_infile}" "${_out_spv}"
          DEPENDS "${_infile}"
          COMMENT "Copy prebuilt SPIR-V: ${_infile} -> ${_out_spv}"
          VERBATIM
        )
      else()
        # skip non-spv sources when BUILD_SHADERS=OFF
        continue()
      endif()
    endif()

    # Embed the .spv into a C source using xxd -i (makes symbols like "<name>_spv" and "<name>_spv_len")
    add_custom_command(
      OUTPUT "${_out_c}"
      COMMAND ${CMAKE_COMMAND} -E echo "/* Auto-generated shader embed for ${_inname} */" > "${_out_c}"
      COMMAND ${XXD_EXEC} -i "${_out_spv}" >> "${_out_c}"
      DEPENDS "${_out_spv}"
      COMMENT "Embedding SPIR-V ${_out_spv} -> ${_out_c}"
      VERBATIM
    )

    list(APPEND _shader_sources "${_out_c}")
  endforeach()
endif()

# Create a phony target that generates shader C files
if(_shader_sources)
  add_custom_target(generate_shaders DEPENDS ${_shader_sources})
else()
  add_custom_target(generate_shaders)
endif()

# --- Collect project sources (all C files under src/) ---
file(GLOB_RECURSE PROJECT_SRCS
  "${CMAKE_SOURCE_DIR}/src/*.c"
)

# Check: if bc_emulate.c references shader symbols like bc2_shader_spv, ensure generated files are included
# We'll create an object target that depends on shader generation and then add everything to the final target.
set(ALL_SRCS ${PROJECT_SRCS} ${_shader_sources})

# --- Create the main target (exynostools). Adjust type if your project previously used SHARED. ---
add_executable(exynostools ${ALL_SRCS})
add_dependencies(exynostools generate_shaders)

# --- Include directories ---
target_include_directories(exynostools PRIVATE
  "${CMAKE_SOURCE_DIR}/include"
  "${GENERATED_SHADER_DIR}"
)

# --- Compiler and link flags --- (conservative defaults, modify if you have special cross flags)
if(NOT MSVC)
  target_compile_options(exynostools PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# --- Find Vulkan --- try the CMake FindVulkan module first; fallback to common patterns ---
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
  message(STATUS "Found Vulkan via find_package: ${Vulkan_LIBRARY}")
  target_include_directories(exynostools PRIVATE ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(exynostools PRIVATE ${Vulkan_LIBRARIES})
else()
  # Fallback: try to find a libvulkan.so on the system
  find_library(LIBVULKAN_NEEDED NAMES vulkan libvulkan vulkan-1 HINTS /usr/lib /usr/local/lib /lib)
  if(LIBVULKAN_NEEDED)
    message(STATUS "Linking with Vulkan library: ${LIBVULKAN_NEEDED}")
    target_link_libraries(exynostools PRIVATE ${LIBVULKAN_NEEDED})
  else()
    message(WARNING "Vulkan not found. You may need to set Vulkan SDK paths or install libvulkan. Build may fail if Vulkan symbols are needed.")
  endif()
endif()

# Link recommended system libs (adjust per platform)
if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(exynostools PRIVATE ${CMAKE_THREAD_LIBS_INIT})
  # add dl if needed by loader operations
  find_library(LIBDL dl)
  if(LIBDL)
    target_link_libraries(exynostools PRIVATE ${LIBDL})
  endif()
endif()

# --- Final packaging-friendly flags ---
set_target_properties(exynostools PROPERTIES
  OUTPUT_NAME "exynostools"
)

# --- Install / packaging rules can be added here if you want ---
message(STATUS "CMake configured. BUILD_SHADERS=${BUILD_SHADERS}. Generated shader directory: ${GENERATED_SHADER_DIR}")
