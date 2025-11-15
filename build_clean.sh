#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build-android}"
echo "Cleaning and building in ${BUILD_DIR}"

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake -S .. -B . -D CMAKE_BUILD_TYPE=Release
cmake --build . -- -j$(nproc || echo 2)
