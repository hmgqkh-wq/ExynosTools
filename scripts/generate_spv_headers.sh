#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Generate SPIR-V for all shaders and emit headers into include/
# Fixes: use glslangValidator -I<dir> (no space) and Vulkan (-V).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SH_DIR="$ROOT/assets/shaders/src"
OUT_DIR="$ROOT/include"
TMP_DIR="$ROOT/build_generated_spv"
PY="$ROOT/cmake/emit_spv_header.py"

mkdir -p "$OUT_DIR" "$TMP_DIR"

command -v glslangValidator >/dev/null 2>&1 || { echo "glslangValidator not found. Install glslang-tools."; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "python3 not found."; exit 1; }

for src in "$SH_DIR"/*.{comp,glsl}; do
  [ -f "$src" ] || continue
  base="$(basename "$src")"
  name="${base%%.*}"
  spv="$TMP_DIR/${name}.spv"
  hdr="$OUT_DIR/${name}_shader.h"

  echo "Compiling ${base} -> ${spv}"
  # Important: -I<dir> (no space). Vulkan mode (-V).
  glslangValidator -V "-I${SH_DIR}" -o "$spv" "$src"

  echo "Emitting header ${hdr}"
  python3 "$PY" "$spv" "$hdr" "${name}_spv"
done

echo "Headers in: $OUT_DIR"
ls -la "$OUT_DIR" || true
