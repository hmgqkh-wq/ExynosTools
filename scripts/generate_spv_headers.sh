#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Generate Vulkan SPIR-V and emit C headers; optionally run spirv-opt.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/assets/shaders/src"
INC="$ROOT/include"
TMP="$ROOT/build_generated_spv"
PY="$ROOT/cmake/emit_spv_header.py"

mkdir -p "$INC" "$TMP"
command -v glslangValidator >/dev/null 2>&1 || { echo "glslangValidator not found. Install glslang-tools."; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "python3 not found."; exit 1; }
SPIRV_OPT_BIN="$(command -v spirv-opt || true)"

for f in "$SRC"/*.{comp,glsl}; do
  [ -f "$f" ] || continue
  name="$(basename "$f")"; base="${name%%.*}"
  raw="$TMP/${base}.spv"; opt="$TMP/${base}.opt.spv"; out="$raw"
  echo "Compiling $name -> $raw"
  glslangValidator -V --target-env vulkan1.2 "-I${SRC}" -o "$raw" "$f"
  if [ -n "$SPIRV_OPT_BIN" ]; then
    echo "Optimizing $raw -> $opt"
    "$SPIRV_OPT_BIN" --strip-debug -O "$raw" -o "$opt" || true
    [ -f "$opt" ] && out="$opt"
  fi
  hdr="$INC/${base}_shader.h"
  echo "Emit header $hdr"
  python3 "$PY" "$out" "$hdr" "${base}_spv"
done

echo "Headers in $INC"
ls -la "$INC" || true
