#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build_release"
DIST_DIR="${SCRIPT_DIR}/dist"

mkdir -p "${BUILD_DIR}" "${DIST_DIR}"

cd "${BUILD_DIR}"

cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_FLAGS="-O2 -DNDEBUG" \
      -DCMAKE_CXX_FLAGS="-O2 -DNDEBUG -static-libgcc -static-libstdc++" \
      -DCMAKE_EXE_LINKER_FLAGS="-s" \
      ..

make -j$(nproc)

strip TextExtractionCLI/TextExtraction

if command -v upx &> /dev/null; then
    upx --best TextExtractionCLI/TextExtraction
else
    echo "UPX not found, skipping compression"
fi

cp TextExtractionCLI/TextExtraction "${DIST_DIR}/TextExtraction-linux-x64"

echo "Build complete: ${DIST_DIR}/TextExtraction-linux-x64"
ls -lh "${DIST_DIR}/TextExtraction-linux-x64"
