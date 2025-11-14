#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Serial, defensive SPIR-V generation: compile -> validate -> optional optimize -> emit header.
# Run via bash (executable bit not required): bash scripts/generate_spv_headers.sh
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
SPIRV_VAL_BIN="$(command -v spirv-val || true)"

echo "Generator starting. Shader dir: $SH_DIR"
[ -n "$SPIRV_OPT_BIN" ] && echo "spirv-opt: $SPIRV_OPT_BIN" || echo "spirv-opt: not found (skipping optimization)"
[ -n "$SPIRV_VAL_BIN" ] && echo "spirv-val: $SPIRV_VAL_BIN" || echo "spirv-val: not found (skipping validation)"

# Compile only entry-point shaders; skip include-only files like bc_common.glsl
for src in "$SH_DIR"/*.{comp,glsl}; do
  [ -f "$src" ] || continue
  name="$(basename "$src")"
  base="${name%%.*}"

  # Skip shared include file
  if [ "$name" = "bc_common.glsl" ]; then
    echo "Skipping include-only file: $name"
    continue
  fi

  raw="$TMP_DIR/${base}.spv"
  opt="$TMP_DIR/${base}.opt.spv"
  out="$raw"

  echo
  echo "=== Processing shader: $name ==="
  echo "1) Compile -> SPV"
  if ! glslangValidator -V --target-env vulkan1.2 "-I${SH_DIR}" -o "$raw" "$src"; then
    echo "ERROR: glslangValidator failed for $src"
    exit 1
  fi

  if [ ! -s "$raw" ]; then
    echo "ERROR: Generated SPV empty: $raw"
    exit 1
  fi
  rem=$(( $(stat -c%s "$raw") % 4 ))
  if [ "$rem" -ne 0 ]; then
    echo "ERROR: SPV size not multiple of 4: $raw (size $(stat -c%s "$raw"))"
    exit 1
  fi

  if [ -n "$SPIRV_VAL_BIN" ]; then
    echo "2) Validate SPV with spirv-val"
    if ! "$SPIRV_VAL_BIN" "$raw"; then
      echo "WARNING: spirv-val failed for $raw; attempting retry"
      cp "$raw" "${raw}.retry"
      if ! "$SPIRV_VAL_BIN" "${raw}.retry"; then
        echo "ERROR: spirv-val still failing for $raw"
        hexdump -n 200 -C "$raw" || true
        exit 1
      else
        echo "spirv-val passed on retry"
      fi
    fi
  fi

  if [ -n "$SPIRV_OPT_BIN" ]; then
    echo "3) Optimize SPV with spirv-opt"
    if "$SPIRV_OPT_BIN" --strip-debug -O "$raw" -o "$opt"; then
      echo "spirv-opt succeeded -> $opt"
      out="$opt"
      if [ -n "$SPIRV_VAL_BIN" ]; then
        if ! "$SPIRV_VAL_BIN" "$out"; then
          echo "WARNING: spirv-val failed on optimized SPV; using unoptimized SPV"
          out="$raw"
        fi
      fi
    else
      echo "WARNING: spirv-opt failed for $raw; continuing with raw SPV"
      out="$raw"
    fi
  fi

  echo "4) Emit C header from SPV -> $OUT_DIR/${base}_shader.h"
  if ! python3 "$PY" "$out" "$OUT_DIR/${base}_shader.h" "${base}_spv"; then
    echo "ERROR: emit_spv_header.py failed for $out"
    exit 1
  fi

  echo "=== Done: $name ==="
done

echo
echo "All shaders processed. Headers in: $OUT_DIR"
ls -la "$OUT_DIR" || true
