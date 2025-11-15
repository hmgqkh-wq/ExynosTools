cmake_minimum_required(VERSION 3.16)
project(ExynosTools C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Paths
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
set(INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/src")
set(GENERATED_SPV_DIR "${CMAKE_BINARY_DIR}/generated_spv")
set(GENERATED_HDR_DIR "${GENERATED_SPV_DIR}")

# Ensure generated headers are visible to compiler
include_directories(
  "${INCLUDE_DIR}"
  "${SRC_DIR}"
  "${GENERATED_HDR_DIR}"
)

# Compiler options
add_compile_options(-Wall -Wextra -Wno-unused-parameter $<$<CONFIG:Release>:-O3>)

# Shader list
set(SHADERS bc1 bc2 bc3 bc4 bc5 bc6h bc7)

# Rule: compile each .comp to .spv then to _shader.h using python3 script.
find_program(GLSLANG_VALIDATOR glslangValidator)
if(NOT GLSLANG_VALIDATOR)
  message(STATUS "glslangValidator not found in PATH; CI sets it up. Local dev may need to install it.")
endif()

file(MAKE_DIRECTORY ${GENERATED_SPV_DIR})

foreach(name ${SHADERS})
  set(src "${SHADER_SRC_DIR}/${name}.comp")
  set(spv "${GENERATED_SPV_DIR}/${name}.spv")
  set(hdr "${GENERATED_HDR_DIR}/${name}_shader.h")
  add_custom_command(
    OUTPUT ${spv} ${hdr}
    COMMAND ${CMAKE_COMMAND} -E echo "Compiling shader ${name}"
    COMMAND ${GLSLANG_VALIDATOR} -V -o ${spv} ${src}
    COMMAND ${CMAKE_COMMAND} -E env python3 ${CMAKE_SOURCE_DIR}/scripts/spv_to_header.py ${spv} ${hdr} ${name}_shader
    DEPENDS ${src} ${CMAKE_SOURCE_DIR}/scripts/spv_to_header.py
    COMMENT "Generate SPIR-V and header for ${name}"
    VERBATIM
  )
  list(APPEND GENERATED_SPV_FILES ${spv} ${hdr})
endforeach()

add_custom_target(gen_shaders ALL DEPENDS ${GENERATED_SPV_FILES})

# Source files
file(GLOB_RECURSE PROJECT_SOURCES "${SRC_DIR}/*.c")
add_executable(exynostools ${PROJECT_SOURCES})

add_dependencies(exynostools gen_shaders)

target_compile_definitions(exynostools PRIVATE
  EXYNOSTOOLS_STAGING_POOL_SIZE=1048576
  XCLIPSE_940_OPTIMIZE=1
)

# Vulkan linking (best-effort)
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(VULKAN_PKG QUIET vulkan)
  if(VULKAN_PKG_FOUND)
    target_include_directories(exynostools PRIVATE ${VULKAN_PKG_INCLUDE_DIRS})
    target_link_libraries(exynostools PRIVATE ${VULKAN_PKG_LDFLAGS})
  else()
    find_library(VULKAN_LIB NAMES vulkan Vulkan)
    if(VULKAN_LIB)
      target_link_libraries(exynostools PRIVATE ${VULKAN_LIB})
    endif()
  endif()
else()
  find_library(VULKAN_LIB NAMES vulkan Vulkan)
  if(VULKAN_LIB)
    target_link_libraries(exynostools PRIVATE ${VULKAN_LIB})
  endif()
endif()

find_package(Threads)
if(THREADS_FOUND)
  target_link_libraries(exynostools PRIVATE Threads::Threads)
endif()

message(STATUS "Configured ExynosTools project with shader generation")
