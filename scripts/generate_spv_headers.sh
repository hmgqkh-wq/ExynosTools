#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# scripts/generate_spv_headers.sh
# Produces include/<name>_shader.h from assets/shaders/src/<name>.comp or .glsl
# Requires: glslangValidator, python3
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SHADERS_DIR="$ROOT/assets/shaders/src"
OUT_DIR="$ROOT/include"
TMP_DIR="$ROOT/build_generated_spv"
mkdir -p "$OUT_DIR" "$TMP_DIR"

command -v glslangValidator >/dev/null 2>&1 || { echo "glslangValidator not found; install glslang-tools"; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "python3 not found; install python3"; exit 1; }

for src in "$SHADERS_DIR"/*.{comp,glsl}; do
  [ -f "$src" ] || continue
  base="$(basename "$src")"
  name="${base%%.*}"              # e.g., bc1, bc_common
  spv="$TMP_DIR/${name}.spv"
  hdr="$OUT_DIR/${name}_shader.h"
  echo "Compiling $src -> $spv"
  glslangValidator -V "$src" -o "$spv"
  echo "Emitting header $hdr"
  python3 "${ROOT}/cmake/emit_spv_header.py" "$spv" "$hdr" "${name}_spv"
done

echo "Generated headers in: $OUT_DIR"
ls -la "$OUT_DIR"
