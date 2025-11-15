#!/usr/bin/env bash
# normalize_and_validate_shaders.sh
# - Removes BOM from shader files
# - Converts CRLF to LF
# - Runs glslangValidator -V on each shader and exits non-zero on any failure
# Usage:
#   bash normalize_and_validate_shaders.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${ROOT}/src"
shopt -s nullglob

# Find shader files
mapfile -t SHADERS < <(find "${SRC_DIR}" -type f \( -iname "*.comp" -o -iname "*.frag" -o -iname "*.vert" -o -iname "*.glsl" \) )

if [ ${#SHADERS[@]} -eq 0 ]; then
  echo "No shader files found under ${SRC_DIR}"
  exit 0
fi

MISSING_TOOL=0
if ! command -v glslangValidator >/dev/null 2>&1; then
  echo "glslangValidator not found in PATH. Install it or add it to the CI runner." >&2
  MISSING_TOOL=1
fi
if ! command -v dos2unix >/dev/null 2>&1; then
  echo "dos2unix not found in PATH. Attempting built-in CRLF normalization." >&2
fi

if [ "${MISSING_TOOL}" -eq 1 ]; then
  echo "Shader validation skipped because glslangValidator is missing." >&2
  exit 2
fi

echo "Normalizing and validating ${#SHADERS[@]} shader(s)..."

for f in "${SHADERS[@]}"; do
  echo "-> Processing: ${f}"
  # Remove UTF-8 BOM if present
  # (0xEF 0xBB 0xBF)
  if head -c 3 "${f}" | grep -q $'\xEF\xBB\xBF'; then
    echo "   Removing BOM from ${f}"
    tail -c +4 "${f}" > "${f}.nobom"
    mv "${f}.nobom" "${f}"
  fi

  # Normalize CRLF -> LF
  if command -v dos2unix >/dev/null 2>&1; then
    dos2unix "${f}" >/dev/null 2>&1 || true
  else
    # fallback: simple perl in-place conversion (handles CRLF)
    perl -pi -e 's/\r\n/\n/g' "${f}"
  fi

  # Ensure first non-blank line is a #version directive for Vulkan GLSL
  first_nonblank=$(sed -n '1,12p' "${f}" | sed -n '/[^[:space:]]/p' | sed -n '1p' || true)
  if [ -n "${first_nonblank}" ] && [[ ! "${first_nonblank}" =~ ^#version ]]; then
    echo "   Warning: ${f} first non-blank line is not a #version directive: ${first_nonblank}"
  fi

  # Run validator
  echo "   Validating ${f}"
  if ! glslangValidator -V "${f}" -o /dev/null; then
    echo "ERROR: glslangValidator failed for ${f}" >&2
    exit 3
  fi
done

echo "All shaders validated successfully."
exit 0
