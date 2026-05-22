#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_macos"
DIST_DIR="${SCRIPT_DIR}/dist"

mkdir -p "${BUILD_DIR}" "${DIST_DIR}"

cd "${BUILD_DIR}"

cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="-O2 -DNDEBUG" \
      -DCMAKE_CXX_FLAGS="-O2 -DNDEBUG" \
      -DCMAKE_EXE_LINKER_FLAGS="-s" \
      ..

make -j$(sysctl -n hw.ncpu)

strip TextExtractionCLI/TextExtraction

if command -v upx &> /dev/null; then
    upx --best TextExtractionCLI/TextExtraction
else
    echo "UPX not found, skipping compression. Install with: brew install upx"
fi

cp TextExtractionCLI/TextExtraction "${DIST_DIR}/TextExtraction-macos-x64"

echo "Build complete: ${DIST_DIR}/TextExtraction-macos-x64"
ls -lh "${DIST_DIR}/TextExtraction-macos-x64"
