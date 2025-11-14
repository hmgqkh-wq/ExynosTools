#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Defensive SPIR-V generator with per-shader captured logs.
# Run with: bash scripts/generate_spv_headers.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SH_DIR="$ROOT/assets/shaders/src"
OUT_DIR="$ROOT/include"
TMP_DIR="$ROOT/build_generated_spv"
PY="$ROOT/cmake/emit_spv_header.py"

mkdir -p "$OUT_DIR" "$TMP_DIR" "$TMP_DIR/logs"

command -v glslangValidator >/dev/null 2>&1 || { echo "ERROR: glslangValidator not found. Install glslang-tools."; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "ERROR: python3 not found."; exit 1; }

SPIRV_OPT_BIN="$(command -v spirv-opt || true)"
SPIRV_VAL_BIN="$(command -v spirv-val || true)"

echo "Generator starting. Shader dir: $SH_DIR"
[ -n "$SPIRV_OPT_BIN" ] && echo "spirv-opt: $SPIRV_OPT_BIN" || echo "spirv-opt: not found (skipping optimization)"
[ -n "$SPIRV_VAL_BIN" ] && echo "spirv-val: $SPIRV_VAL_BIN" || echo "spirv-val: not found (skipping validation)"

for src in "$SH_DIR"/*.{comp,glsl}; do
  [ -f "$src" ] || continue
  name="$(basename "$src")"
  base="${name%%.*}"

  # Skip known include-only helpers
  case "$name" in
    bc_common.glsl|common.glsl|shared.glsl) echo "Skipping include-only: $name"; continue ;;
  esac

  raw="$TMP_DIR/${base}.spv"
  opt="$TMP_DIR/${base}.opt.spv"
  out="$raw"
  log="$TMP_DIR/logs/${base}.log"

  echo
  echo "=== Processing shader: $name ==="
  echo "Logs -> $log"
  rm -f "$log"

  echo "1) Compile -> SPV"
  if ! glslangValidator -V --target-env vulkan1.2 "-I${SH_DIR}" -o "$raw" "$src" 2> "${log}.glslang.stderr"; then
    echo "ERROR: glslangValidator failed for $src"
    echo "----- glslangValidator stderr (trimmed) -----"
    tail -n 200 "${log}.glslang.stderr" || true
    echo "----- end glslang stderr -----"
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
    if ! "$SPIRV_VAL_BIN" "$raw" > "${log}.spirv-val.stdout" 2> "${log}.spirv-val.stderr"; then
      echo "ERROR: spirv-val failed for $raw"
      echo "----- spirv-val stderr -----"
      tail -n 200 "${log}.spirv-val.stderr" || true
      echo "----- end spirv-val stderr -----"
      exit 1
    fi
  fi

  if [ -n "$SPIRV_OPT_BIN" ]; then
    echo "3) Optimize SPV with spirv-opt"
    if "$SPIRV_OPT_BIN" --strip-debug -O "$raw" -o "$opt" > "${log}.spv-opt.stdout" 2> "${log}.spv-opt.stderr"; then
      echo "spirv-opt succeeded -> $opt"
      out="$opt"
      if [ -n "$SPIRV_VAL_BIN" ]; then
        if ! "$SPIRV_VAL_BIN" "$out" > "${log}.spirv-val.opt.stdout" 2> "${log}.spirv-val.opt.stderr"; then
          echo "WARNING: spirv-val failed on optimized SPV; falling back to raw SPV"
          echo "----- spirv-val (opt) stderr -----"
          tail -n 200 "${log}.spirv-val.opt.stderr" || true
          echo "----- end spirv-val (opt) stderr -----"
          out="$raw"
        fi
      fi
    else
      echo "WARNING: spirv-opt failed for $raw; see ${log}.spv-opt.stderr; continuing with raw SPV"
      tail -n 200 "${log}.spv-opt.stderr" || true
      out="$raw"
    fi
  fi

  echo "4) Emit C header from SPV -> $OUT_DIR/${base}_shader.h"
  if ! python3 "$PY" "$out" "$OUT_DIR/${base}_shader.h" "${base}_spv" > "${log}.emit.stdout" 2> "${log}.emit.stderr"; then
    echo "ERROR: emit_spv_header.py failed for $out"
    echo "----- emit_spv_header stderr -----"
    tail -n 200 "${log}.emit.stderr" || true
    echo "----- end emit_spv_header stderr -----"
    exit 1
  fi

  echo "=== Done: $name (logs in ${log}.*) ==="
done

echo
echo "All shaders processed. Headers in: $OUT_DIR"
ls -la "$OUT_DIR" || true
