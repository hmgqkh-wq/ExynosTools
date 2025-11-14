cmake_minimum_required(VERSION 3.16)
project(ExynosTools LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Include generator early (optional; CI script already generates)
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/generate_shader_headers.cmake")
  include("${CMAKE_SOURCE_DIR}/cmake/generate_shader_headers.cmake")
endif()

include_directories("${CMAKE_SOURCE_DIR}/include" "${CMAKE_BINARY_DIR}/generated_shaders")

file(GLOB_RECURSE EXYNOSTOOLS_SRCS "${CMAKE_SOURCE_DIR}/src/*.c" "${CMAKE_SOURCE_DIR}/src/*.cpp")
add_library(exynostools STATIC ${EXYNOSTOOLS_SRCS})

if(TARGET gen_shaders)
  add_dependencies(exynostools gen_shaders)
endif()

target_include_directories(exynostools PUBLIC
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_BINARY_DIR}/generated_shaders"
)

set_target_properties(exynostools PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(exynostools PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/" DESTINATION include OPTIONAL)
