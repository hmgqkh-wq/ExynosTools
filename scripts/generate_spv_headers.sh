#!/usr/bin/env bash
# scripts/generate_spv_headers.sh
# SPDX-License-Identifier: MIT
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SH_DIR="$ROOT/assets/shaders/src"
OUT_DIR="$ROOT/include"
TMP_DIR="$ROOT/build_generated_spv"
PY="$ROOT/cmake/emit_spv_header.py"

mkdir -p "$OUT_DIR" "$TMP_DIR" "$TMP_DIR/logs"

command -v glslangValidator >/dev/null 2>&1 || { echo "ERROR: glslangValidator not found"; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "ERROR: python3 not found"; exit 1; }
SPIRV_OPT="$(command -v spirv-opt || true)"
SPIRV_VAL="$(command -v spirv-val || true)"

for src in "$SH_DIR"/*.{comp,glsl}; do
  [ -f "$src" ] || continue
  name="$(basename "$src")"
  base="${name%%.*}"
  case "$name" in
    bc_common.glsl|common.glsl|shared.glsl) echo "Skipping include-only: $name"; continue ;;
  esac

  raw="$TMP_DIR/${base}.spv"
  opt="$TMP_DIR/${base}.opt.spv"
  out="$raw"
  log="$TMP_DIR/logs/${base}.log"

  glslangValidator -V --target-env vulkan1.2 "-I${SH_DIR}" -o "$raw" "$src" 2> "${log}.glslang.stderr"

  if [ -n "$SPIRV_VAL" ]; then
    "$SPIRV_VAL" "$raw" > "${log}.spirv-val.stdout" 2> "${log}.spirv-val.stderr" || true
  fi

  if [ -n "$SPIRV_OPT" ]; then
    if "$SPIRV_OPT" --strip-debug --inline-entry-points-exhaustive -O "$raw" -o "$opt" 2> "${log}.spv-opt.stderr"; then
      out="$opt"
      [ -n "$SPIRV_VAL" ] && "$SPIRV_VAL" "$out" > "${log}.spirv-val.opt.stdout" 2> "${log}.spirv-val.opt.stderr" || true
    else
      out="$raw"
    fi
  fi

  sym="${base}_shader_spv"
  python3 "$PY" "$out" "$OUT_DIR/${base}_shader.h" "$sym" > "${log}.emit.stdout" 2> "${log}.emit.stderr" || (tail -n 200 "${log}.emit.stderr" || true; exit 1)
done

ls -la "$OUT_DIR" || true
