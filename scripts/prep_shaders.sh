#!/usr/bin/env bash
# scripts/prep_shaders.sh
# Preprocess shaders before validator:
#  - Ensure '#version' is present; if missing add '#version 450'
#  - Add a small compatibility header guard if desired
#  - Copy sanitized files to generated_shaders for compilation

set -euo pipefail

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <shaders_src_dir> <generated_shader_dir>"
  exit 1
fi

SHADERS_SRC="$1"
GENERATED_DIR="$2"
DEFAULT_VERSION="450"

mkdir -p "${GENERATED_DIR}"

# minimal compat header (inserted before shaders lacking it)
read -r -d '' COMPAT_HEADER <<'HEADER' || true
// Auto-inserted compatibility header for Vulkan compute shaders.
// This header ensures a baseline version and common defines.
#version 450
// enable explicit binding support if needed by shader author
// #extension GL_EXT_nonuniform_qualifier : enable
// #extension GL_ARB_shader_ballot : enable
HEADER

# process each shader (only the text-based ones)
find "${SHADERS_SRC}" -type f \( -name "*.comp" -o -name "*.frag" -o -name "*.vert" -o -name "*.glsl" \) | while read -r sfile; do
  base=$(basename "${sfile}")
  out="${GENERATED_DIR}/${base}"
  # read first non-empty line to check for #version
  first_line="$(awk 'NF{print; exit}' "${sfile}" || true)"
  if [[ "${first_line}" =~ ^#version[[:space:]]+([0-9]+) ]]; then
    # has version: copy as-is (but normalizer can still touch it if needed)
    cp "${sfile}" "${out}"
  else
    # insert compat header
    {
      echo "// Prepended by prep_shaders.sh for ${base}"
      printf "%s\n" "${COMPAT_HEADER}"
      echo ""
      cat "${sfile}"
    } > "${out}"
  fi

  # ensure file uses unix newlines
  if command -v dos2unix >/dev/null 2>&1; then
    dos2unix "${out}" >/dev/null 2>&1 || true
  else
    # fallback: remove CR characters
    sed -i 's/\r$//' "${out}" || true
  fi
  echo "Prepared shader: ${sfile} -> ${out}"
done

# also copy .spv if present (no change)
find "${SHADERS_SRC}" -type f -name "*.spv" -exec cp -u {} "${GENERATED_DIR}/" \;

exit 0
