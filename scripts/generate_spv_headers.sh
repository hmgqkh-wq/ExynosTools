#!/usr/bin/env bash
# scripts/generate_spv_headers.sh
# SPDX-License-Identifier: MIT
# Serial SPIR-V generation + optimized spirv-opt passes suitable for mobile drivers.
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

echo "Shader dir: $SH_DIR"
echo "Out dir: $OUT_DIR"
[ -n "$SPIRV_OPT" ] && echo "spirv-opt: $SPIRV_OPT" || echo "spirv-opt not found (skipping advanced opt)"
[ -n "$SPIRV_VAL" ] && echo "spirv-val: $SPIRV_VAL" || echo "spirv-val not found (skipping validation)"

for src in "$SH_DIR"/*.{comp,glsl}; do
  [ -f "$src" ] || continue
  name="$(basename "$src")"
  base="${name%%.*}"

  # skip common includes
  case "$name" in
    bc_common.glsl|common.glsl|shared.glsl) echo "Skipping include-only: $name"; continue ;;
  esac

  raw="$TMP_DIR/${base}.spv"
  opt="$TMP_DIR/${base}.opt.spv"
  out="$raw"
  log="$TMP_DIR/logs/${base}.log"

  echo
  echo "=== $name ==="
  if ! glslangValidator -V --target-env vulkan1.2 "-I${SH_DIR}" -o "$raw" "$src" 2> "${log}.glslang.stderr"; then
    echo "glslang failed for $src"; tail -n 200 "${log}.glslang.stderr" || true; exit 1
  fi

  if [ -n "$SPIRV_VAL" ]; then
    if ! "$SPIRV_VAL" "$raw" > "${log}.spirv-val.stdout" 2> "${log}.spirv-val.stderr"; then
      echo "spirv-val failed for $raw"; tail -n 200 "${log}.spirv-val.stderr" || true; exit 1
    fi
  fi

  if [ -n "$SPIRV_OPT" ]; then
    # Recommended spirv-opt passes for mobile drivers: inline, freeze-spec-const, strip-debug, optimize
    if "$SPIRV_OPT" --strip-debug --inline-entry-points-exhaustive -O "$raw" -o "$opt" 2> "${log}.spv-opt.stderr"; then
      out="$opt"
      [ -n "$SPIRV_VAL" ] && "$SPIRV_VAL" "$out" > "${log}.spirv-val.opt.stdout" 2> "${log}.spirv-val.opt.stderr" || true
    else
      echo "spirv-opt failed (continuing with raw)"; tail -n 200 "${log}.spv-opt.stderr" || true
      out="$raw"
    fi
  fi

  sym="${base}_shader_spv"
  if ! python3 "$PY" "$out" "$OUT_DIR/${base}_shader.h" "$sym" > "${log}.emit.stdout" 2> "${log}.emit.stderr"; then
    echo "emit_spv_header.py failed for $out"; tail -n 200 "${log}.emit.stderr" || true; exit 1
  fi

  echo "Done: $name -> ${base}_shader.h (symbol: $sym)"
done

echo "All shaders processed. Headers in $OUT_DIR"
ls -la "$OUT_DIR" || true
