# SPDX-License-Identifier: MIT
# cmake/emit_spv_header.cmake
# Usage: cmake -DINPUT_SPV=... -DOUTPUT_H=... -P emit_spv_header.cmake
file(READ "${INPUT_SPV}" SPV_BINARY_RAW HEX)
# Better to read binary and write C array — use a small generator in CMake script:
file(READ "${INPUT_SPV}" SPV_DATA HEX)
# Convert binary to uint32 array (little-endian). We'll use the built-in binary read helper.
file(READ "${INPUT_SPV}" SPV_BIN RELATIVE)
# Simpler: call a tiny python one-liner if python available
execute_process(
  COMMAND ${CMAKE_COMMAND} -E echo_append ""
)
# Fallback: create a minimal header referencing external symbols; actual SPV array should be produced by a CI artifact if python not available
file(WRITE "${OUTPUT_H}" "/* SPDX-License-Identifier: MIT */\n#include <stddef.h>\n#include <stdint.h>\n#ifdef __cplusplus\nextern \"C\" {\n#endif\nextern const uint32_t ${INPUT_SPV}_spv[];\nextern const size_t ${INPUT_SPV}_spv_size;\n#ifdef __cplusplus\n}\n#endif\n")
