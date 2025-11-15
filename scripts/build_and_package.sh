#!/usr/bin/env bash
set -euo pipefail

# ExynosTools v1.3.1 Build and Package Script
# Includes ray tracing, async compile, BCn shaders, HUD, and performance mode

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
INSTALL_DIR="${BUILD_DIR}/install"
ARTIFACTS_DIR="${ROOT_DIR}/artifacts"
PKG_DIR="${INSTALL_DIR}/package_temp"

echo "🔧 ExynosTools v1.3.1 - Build and Package"
echo "Includes async, ray tracing, BCn, HUD, and performance mode..."

# Clean previous builds
rm -rf "${BUILD_DIR}" "${ARTIFACTS_DIR}" "${PKG_DIR}"
mkdir -p "${BUILD_DIR}" "${INSTALL_DIR}" "${ARTIFACTS_DIR}"

# Build shaders
echo "🎨 Preparing shaders..."
"${ROOT_DIR}/scripts/prep_shaders.sh"
"${ROOT_DIR}/scripts/generate_spv_headers.sh"

# Build driver
echo "🔨 Building driver..."
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"
cmake --build "${BUILD_DIR}" --target xeno_wrapper --config Release -j$(nproc)

# Verify binary
BINARY_PATH="${BUILD_DIR}/libxeno_wrapper.so"
if [ ! -f "${BINARY_PATH}" ] || [ "$(stat -c%s "${BINARY_PATH}")" -eq 0 ]; then
    echo "❌ ERROR: libxeno_wrapper.so is missing or empty!"
    exit 1
fi
echo "✅ Verified libxeno_wrapper.so: $(stat -c%s "${BINARY_PATH}") bytes"

# Create package structure
echo "📦 Creating package_temp layout..."
mkdir -p "${PKG_DIR}/usr/lib"
mkdir -p "${PKG_DIR}/usr/share/exynostools/shaders/src"
mkdir -p "${PKG_DIR}/etc/exynostools/profiles/vendor/qcom_adreno/manifests"

# Copy driver
cp -v "${BINARY_PATH}" "${PKG_DIR}/usr/lib/"

# Copy shaders
cp -v "${BUILD_DIR}/generated_shaders/"*.spv "${PKG_DIR}/usr/share/exynostools/shaders/" || true
cp -v "${BUILD_DIR}/generated_shaders/"*_spv.c "${PKG_DIR}/usr/share/exynostools/shaders/" || true
cp -v "${ROOT_DIR}/assets/shaders/src/"* "${PKG_DIR}/usr/share/exynostools/shaders/src/" || true

# Copy config files
cp -v "${ROOT_DIR}/etc/exynostools/performance_mode.conf" "${PKG_DIR}/etc/exynostools/"
cp -v "${ROOT_DIR}/etc/exynostools/profiles/"*.conf "${PKG_DIR}/etc/exynostools/profiles/" || true
cp -v "${ROOT_DIR}/etc/exynostools/profiles/"*.env "${PKG_DIR}/etc/exynostools/profiles/" || true
cp -v "${ROOT_DIR}/etc/exynostools/profiles/vendor/qcom_adreno/manifests/manifest.json" \
    "${PKG_DIR}/etc/exynostools/profiles/vendor/qcom_adreno/manifests/"

# Generate meta.json
cat > "${PKG_DIR}/usr/share/meta.json" <<EOF
{
  "name": "xclipse-940",
  "library": "libxeno_wrapper.so",
  "entry": "xeno_init",
  "target": "vulkan1.3",
  "features": {
    "spirv_embedded": true,
    "pipeline_cache": true,
    "performance_mode": true,
    "ray_tracing": true,
    "async_compile": true
  },
  "assets": {
    "shaders": "usr/share/exynostools/shaders",
    "profiles": "etc/exynostools/profiles"
  }
}
EOF
echo "✅ Generated meta.json"

# Verify contents
echo "📋 Final package contents:"
find "${PKG_DIR}" -type f | sort

# Package
echo "📦 Creating tar.zst archive..."
tar --zstd -cvf "${ARTIFACTS_DIR}/xclipse-940-driver-pack.tar.zst" -C "${PKG_DIR}" .

echo ""
echo "🎉 Package created: ${ARTIFACTS_DIR}/xclipse-940-driver-pack.tar.zst"
echo "✅ Ready for Winlator installation"
