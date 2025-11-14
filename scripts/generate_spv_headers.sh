#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# scripts/generate_spv_headers.sh
# Compile all shaders (Vulkan), optionally run spirv-opt, emit C headers.
# This script runs serially and exits nonzero on any failure.

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SH_DIR="$ROOT/assets/shaders/src"
OUT_DIR="$ROOT/include"
TMP_DIR="$ROOT/build_generated_spv"
PY="$ROOT/cmake/emit_spv_header.py"

mkdir -p "$OUT_DIR" "$TMP_DIR"

command -v glslangValidator >/dev/null 2>&1 || { echo "ERROR: glslangValidator not found. Install glslang-tools."; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "ERROR: python3 not found."; exit 1; }

SPIRV_OPT_BIN="$(command -v spirv-opt || true)"
if [ -n "$SPIRV_OPT_BIN" ]; then
  echo "spirv-opt found at: $SPIRV_OPT_BIN"
else
  echo "spirv-opt not found; skipping SPIR-V optimization"
fi

# Process shaders serially to avoid races
for src in "$SH_DIR"/*.{comp,glsl}; do
  [ -f "$src" ] || continue
  name="$(basename "$src")"
  base="${name%%.*}"
  raw="$TMP_DIR/${base}.spv"
  opt="$TMP_DIR/${base}.opt.spv"
  out="$raw"

  echo "----"
  echo "Compiling: $name"
  glslangValidator -V --target-env vulkan1.2 "-I${SH_DIR}" -o "$raw" "$src"

  if [ -n "$SPIRV_OPT_BIN" ]; then
    echo "Optimizing: $raw -> $opt (via $SPIRV_OPT_BIN)"
    "$SPIRV_OPT_BIN" --strip-debug -O "$raw" -o "$opt" || { echo "ERROR: spirv-opt failed for $raw"; exit 1; }
    [ -f "$opt" ] && out="$opt"
  fi

  hdr="$OUT_DIR/${base}_shader.h"
  echo "Emitting header: $hdr from ${out}"
  python3 "$PY" "$out" "$hdr" "${base}_spv" || { echo "ERROR: emit_spv_header.py failed for $out"; exit 1; }
done

echo "All shaders processed. Headers in: $OUT_DIR"
ls -la "$OUT_DIR" || true
