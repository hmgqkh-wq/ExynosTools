#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# scripts/generate_spv_headers.sh
# Generate SPIR-V and headers; use spirv-opt if available (calls by full path).

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SH_DIR="$ROOT/assets/shaders/src"
OUT_DIR="$ROOT/include"
TMP_DIR="$ROOT/build_generated_spv"
PY="$ROOT/cmake/emit_spv_header.py"

mkdir -p "$OUT_DIR" "$TMP_DIR"

command -v glslangValidator >/dev/null 2>&1 || { echo "glslangValidator not found. Install glslang-tools."; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "python3 not found."; exit 1; }

SPIRV_OPT_BIN="$(command -v spirv-opt || true)"
if [ -n "$SPIRV_OPT_BIN" ]; then
  echo "spirv-opt found at: $SPIRV_OPT_BIN"
else
  echo "spirv-opt not found; skipping SPIR-V optimization"
fi

for src in "$SH_DIR"/*.{comp,glsl}; do
  [ -f "$src" ] || continue
  base="$(basename "$src")"
  name="${base%%.*}"
  raw="$TMP_DIR/${name}.spv"
  opt="$TMP_DIR/${name}.opt.spv"
  out="$raw"

  echo "Compiling ${base} -> ${raw}"
  glslangValidator -V --target-env vulkan1.2 "-I${SH_DIR}" -o "$raw" "$src"

  if [ -n "$SPIRV_OPT_BIN" ]; then
    echo "Optimizing ${raw} -> ${opt} using ${SPIRV_OPT_BIN}"
    "$SPIRV_OPT_BIN" --strip-debug -O "$raw" -o "$opt" || { echo "spirv-opt failed for $raw"; exit 1; }
    if [ -f "$opt" ]; then out="$opt"; fi
  fi

  hdr="$OUT_DIR/${name}_shader.h"
  echo "Emitting header ${hdr} from ${out}"
  python3 "$PY" "$out" "$hdr" "${name}_spv"
done

echo "Headers in: $OUT_DIR"
ls -la "$OUT_DIR" || true
