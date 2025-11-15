#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   ./build_clean.sh build-android
#   ./build_clean.sh build-local
BUILD_DIR="${1:-build}"

echo "Removing and recreating build directory: ${BUILD_DIR}"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure: adjust toolchain or Android NDK variables as needed
# For native build:
cmake -S .. -B . -DCMAKE_BUILD_TYPE=Release

# For Android NDK build example, uncomment and set ANDROID_NDK: 
# cmake -S .. -B . -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-31 -D CMAKE_BUILD_TYPE=Release

# Build
cmake --build . -- -j$(nproc || echo 2)

echo "Build completed in ${BUILD_DIR}"
