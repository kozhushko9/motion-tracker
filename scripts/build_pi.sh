#!/usr/bin/env bash
set -euo pipefail

TARGET_DIR="${1:-$(basename "$PWD")}"
BUILD_TYPE="${2:-Release}"

# Check for valid CMake build types
if [[ ! "$BUILD_TYPE" =~ ^(Debug|Release|RelWithDebInfo|MinSizeRel)$ ]]; then
    echo "Error: Invalid build type '$BUILD_TYPE'."
    echo "Allowed: Debug, Release, RelWithDebInfo, MinSizeRel"
    exit 1
fi

SYSROOT="$(realpath ./sysroot)"
TOOLCHAIN_FILE="$(realpath ./cmake/toolchain-aarch64.cmake)"

BUILD_DIR="./build/$TARGET_DIR"
INSTALL_DIR="./install/$TARGET_DIR"

echo "===> Building Raspberry Pi target: $TARGET_DIR"
echo "===> Build type: $BUILD_TYPE"

rm -rf "$BUILD_DIR" "$INSTALL_DIR"
mkdir -p "$BUILD_DIR" "$INSTALL_DIR"

cmake -S . -B "$BUILD_DIR" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
  -DCMAKE_SYSROOT="$SYSROOT" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_INSTALL_PREFIX=/usr \

JOBS="${JOBS:-$(nproc)}"

cmake --build "$BUILD_DIR" -j"$JOBS"

DESTDIR="$(realpath "$INSTALL_DIR")" cmake --install "$BUILD_DIR"

echo "===> Built and staged: $INSTALL_DIR"
echo "===> Executable: $INSTALL_DIR/usr/bin/motion_tracker"