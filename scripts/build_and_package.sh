#!/usr/bin/env bash
set -euo pipefail

# ExynosTools v1.3.1 - Improved Build & Winlator Packaging Script
# Produces:
#  - a turnip-style driver package rooted under package_temp/usr
#  - a .tar.zst artifact for direct install
#  - a Winlator-friendly .wcp (zip) package in artifacts/
#
# Expectations:
#  - repo root contains CMakeLists.txt and src/, drivers/, assets/, etc.
#  - required tools: cmake, make, gcc/clang, xxd, tar, zstd
#  - optional tools: glslangValidator or glslc for shader compilation
#
# This script tries to be robust on Linux host (x86_64) cross-compile to aarch64
# or native aarch64 build. For Android-device builds via Termux / NDK you may
# need to adjust CC/CFLAGS and toolchain files.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
INSTALL_DIR="${BUILD_DIR}/install"
ARTIFACTS_DIR="${ROOT_DIR}/artifacts"
PKG_DIR="${INSTALL_DIR}/package_temp"

# Winlator/Turnip canonical library path inside packages
LIB_DIR_REL="usr/lib/aarch64-linux-gnu"
LIB_DIR="${PKG_DIR}/${LIB_DIR_REL}"
SHADERS_REL="usr/share/exynostools/shaders"
SHADERS_DIR="${PKG_DIR}/${SHADERS_REL}"
PROFILES_REL="etc/exynostools/profiles"
PROFILES_DIR="${PKG_DIR}/${PROFILES_REL}"

echo "🔧 ExynosTools Build & Winlator Packaging"
echo "Root: ${ROOT_DIR}"
echo "Build dir: ${BUILD_DIR}"
echo "Install staging: ${PKG_DIR}"
echo ""

# Clean previous runs
rm -rf "${BUILD_DIR}" "${ARTIFACTS_DIR}" "${PKG_DIR}"
mkdir -p "${BUILD_DIR}" "${INSTALL_DIR}" "${ARTIFACTS_DIR}" "${PKG_DIR}"

# Check minimal toolchain
command -v cmake >/dev/null 2>&1 || { echo "cmake missing; install it."; exit 1; }
command -v tar >/dev/null 2>&1 || { echo "tar missing; install it."; exit 1; }
command -v zstd >/dev/null 2>&1 || { echo "zstd missing; install it."; exit 1; }
command -v xxd >/dev/null 2>&1 || { echo "xxd missing; install (often in vim-common)"; exit 1; }

# Optional shader compilers
GLSLANG_VALIDATOR="$(command -v glslangValidator || true)"
GLSLC_EXEC="$(command -v glslc || true)"
if [ -z "${GLSLANG_VALIDATOR}${GLSLC_EXEC}" ]; then
  echo "⚠️  No glslangValidator or glslc found. BUILD_SHADERS will be skipped by CMake if not available."
fi

# Prep scripts (if present)
if [ -x "${ROOT_DIR}/scripts/prep_shaders.sh" ]; then
  echo "🎨 Running prep_shaders.sh"
  "${ROOT_DIR}/scripts/prep_shaders.sh"
fi

if [ -x "${ROOT_DIR}/scripts/generate_spv_headers.sh" ]; then
  echo "🎨 Running generate_spv_headers.sh"
  "${ROOT_DIR}/scripts/generate_spv_headers.sh"
fi

echo "🔨 Configuring CMake build..."
# Respect CC/CFLAGS environment override if user set them; otherwise default
CMAKE_OPTS=(
  -S "${ROOT_DIR}"
  -B "${BUILD_DIR}"
  -DCMAKE_BUILD_TYPE=Release
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"
  -DBUILD_SHADERS=ON
  -DENABLE_LTO=ON
)
# If shader compiler found, hint CMake which to use
if [ -n "${GLSLANG_VALIDATOR}" ]; then
  CMAKE_OPTS+=("-DGLSLANG_VALIDATOR=${GLSLANG_VALIDATOR}")
elif [ -n "${GLSLC_EXEC}" ]; then
  CMAKE_OPTS+=("-DGLSLC_EXEC=${GLSLC_EXEC}")
fi

# Generate build system
cmake "${CMAKE_OPTS[@]}"

echo "🔧 Building library targets..."
# build wrapper library and optionally tools
cmake --build "${BUILD_DIR}" --target xeno_wrapper -- -j"$(nproc)" || {
  echo "❌ Build failed for xeno_wrapper. See CMake output above."
  exit 1
}

# If there is a host tools target (exynostools) attempt it too for packaging tools
if cmake --build "${BUILD_DIR}" --target exynostools -j1 -- VERBOSE=1 >/dev/null 2>&1; then
  echo "ℹ️  exynostools target available; building."
  cmake --build "${BUILD_DIR}" --target exynostools -- -j"$(nproc)" || true
fi

echo "✅ Build complete."

# Locate produced library. prefer libxeno_wrapper.so then fallback to libvulkan_*-style
LIB_CANDIDATES=(
  "${BUILD_DIR}/libxeno_wrapper.so"
  "${BUILD_DIR}/xeno_wrapper/libxeno_wrapper.so"
  "${BUILD_DIR}/libvulkan_xeno.so"
  "${BUILD_DIR}/libvulkan_xeno_wrapper.so"
)

LIB_PATH=""
for p in "${LIB_CANDIDATES[@]}"; do
  if [ -f "${p}" ]; then
    LIB_PATH="${p}"
    break
  fi
done

if [ -z "${LIB_PATH}" ]; then
  echo "❌ Could not find built library (libxeno_wrapper.so). Looked in: ${LIB_CANDIDATES[*]}"
  exit 1
fi

echo "📦 Library found: ${LIB_PATH} ($(stat -c%s "${LIB_PATH}") bytes)"

# Create package directories (Winlator / Turnip layout expectations)
mkdir -p "${LIB_DIR}"
mkdir -p "${SHADERS_DIR}"
mkdir -p "${PROFILES_DIR}"
mkdir -p "${PKG_DIR}/usr/share/exynostools/shaders/pipeline_cache"

# Copy the driver into the aarch64 library dir (Winlator expects arch-specific libs)
cp -v "${LIB_PATH}" "${LIB_DIR}/" || { echo "Failed to copy library"; exit 1; }

# Create a friendly symlink for other conventions (libxeno_wrapper.so -> libvulkan_xeno.so)
pushd "${LIB_DIR}" >/dev/null
if [ ! -f "libvulkan_xeno.so" ]; then
  ln -sf "libxeno_wrapper.so" "libvulkan_xeno.so" || true
fi
popd >/dev/null

# Copy shaders (if generated)
if [ -d "${BUILD_DIR}/generated_shaders" ]; then
  cp -v "${BUILD_DIR}/generated_shaders/"*.spv "${SHADERS_DIR}/" 2>/dev/null || true
  cp -v "${BUILD_DIR}/generated_shaders/"*_spv.c "${SHADERS_DIR}/" 2>/dev/null || true
fi

# Copy source shader assets (for debugging)
if [ -d "${ROOT_DIR}/assets/shaders/src" ]; then
  cp -rv "${ROOT_DIR}/assets/shaders/src/"* "${SHADERS_DIR}/" || true
fi

# Copy profiles and manifest if present in repo etc/
if [ -d "${ROOT_DIR}/etc/exynostools/profiles" ]; then
  cp -rv "${ROOT_DIR}/etc/exynostools/profiles/"* "${PROFILES_DIR}/" || true
fi

# Deploy the user-provided manifest (if present)
if [ -f "${ROOT_DIR}/manifests/xclipse-940-manifest.json" ]; then
  mkdir -p "${PKG_DIR}/etc/exynostools/profiles/vendor/xilinx_xc" || true
  cp -v "${ROOT_DIR}/manifests/xclipse-940-manifest.json" "${PKG_DIR}/etc/exynostools/profiles/vendor/xilinx_xc/manifest.json" || true
fi

# Copy config and performance mode
if [ -f "${ROOT_DIR}/etc/exynostools/performance_mode.conf" ]; then
  mkdir -p "${PKG_DIR}/etc/exynostools"
  cp -v "${ROOT_DIR}/etc/exynostools/performance_mode.conf" "${PKG_DIR}/etc/exynostools/" || true
fi

# Generate meta.json used by packaging + Winlator hooks
cat > "${PKG_DIR}/usr/share/exynostools/meta.json" <<'EOF'
{
  "name": "xclipse-940",
  "library": "libvulkan_xeno.so",
  "entry": "xeno_init",
  "target": "vulkan1.4",
  "features": {
    "spirv_embedded": true,
    "pipeline_cache": true,
    "performance_mode": true,
    "ray_tracing": true,
    "async_compile": true,
    "variable_rate_shading": true,
    "mesh_shading": true
  },
  "assets": {
    "shaders": "usr/share/exynostools/shaders",
    "profiles": "etc/exynostools/profiles"
  }
}
EOF
echo "✅ meta.json created at ${PKG_DIR}/usr/share/exynostools/meta.json"

# Also copy the user's manifest into the package root for Winlator to read if present
if [ -f "${ROOT_DIR}/manifests/xclipse-940-manifest.json" ]; then
  mkdir -p "${PKG_DIR}/usr/share/exynostools/manifests"
  cp -v "${ROOT_DIR}/manifests/xclipse-940-manifest.json" "${PKG_DIR}/usr/share/exynostools/manifests/" || true
fi

# Record final package contents
echo ""
echo "📋 Final package contents:"
find "${PKG_DIR}" -type f | sort

# Create Winlator-friendly .wcp (zip) and tar.zst artifacts
ART_TAR="${ARTIFACTS_DIR}/xclipse-940-driver-pack.tar.zst"
ART_WCP="${ARTIFACTS_DIR}/xclipse-940-driver-pack.wcp.zip"

echo ""
echo "📦 Creating tar.zst: ${ART_TAR}"
tar --zstd -cvf "${ART_TAR}" -C "${PKG_DIR}" . >/dev/null

echo "📦 Creating Winlator .wcp zip: ${ART_WCP}"
# .wcp is essentially a zip with Winlator-friendly structure; ensure deterministic packaging
pushd "${PKG_DIR}" >/dev/null
zip -r -q "${ART_WCP}" ./*
popd >/dev/null

echo ""
echo "🎉 Packaging complete."
echo "Artifacts:"
ls -lh "${ARTIFACTS_DIR}" || true
echo ""
echo "Instruction: copy ${ART_TAR} or ${ART_WCP} to your Winlator content path or use the Turnip driver install method."
