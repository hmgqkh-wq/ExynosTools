#!/usr/bin/env bash
set -euo pipefail

ANDROID_NDK_PATH="${ANDROID_NDK_PATH:-/path/to/android-ndk}"
ABI="${ABI:-arm64-v8a}"
ANDROID_API="${ANDROID_API:-31}"
BUILD_DIR="${1:-build-android}"

if [ ! -d "${ANDROID_NDK_PATH}" ]; then
  echo "ERROR: set ANDROID_NDK_PATH environment variable to a valid NDK path"
  exit 1
fi

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake -S .. -B . \
  -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_PATH}/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="${ABI}" \
  -DANDROID_PLATFORM="android-${ANDROID_API}" \
  -D CMAKE_BUILD_TYPE=Release

cmake --build . -- -j$(nproc || echo 2)

echo "Android build finished in ${BUILD_DIR}"
