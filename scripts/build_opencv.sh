#!/usr/bin/env bash
set -euo pipefail

OPENCV_BRANCH="${OPENCV_BRANCH:-4.13.0}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

OPENCV_DEV_DIR="${OPENCV_DEV_DIR:-$HOME/external_builds/OpenCV}"
OPENCV_SRC_DIR="${OPENCV_SRC_DIR:-${OPENCV_DEV_DIR}/opencv}"

SYSROOT="${PROJECT_ROOT}/sysroot"
TOOLCHAIN_FILE="${PROJECT_ROOT}/cmake/toolchain-aarch64.cmake"

BUILD_DIR="${PROJECT_ROOT}/build/opencv/aarch64-linux-gnu"
OPENCV_INSTALL_DIR="${PROJECT_ROOT}/third_party/opencv/aarch64-linux-gnu"

echo "===> Building OpenCV for Raspberry Pi Zero 2W / aarch64"
echo "===> Source: ${OPENCV_SRC_DIR}"
echo "===> Branch: ${OPENCV_BRANCH}"
echo "===> Install: ${OPENCV_INSTALL_DIR}"

if [[ ! -d "${SYSROOT}" ]]; then
    echo "ERROR: Sysroot not found: ${SYSROOT}"
    echo "Run: ./scripts/sync_sysroot.sh <PI_HOST>"
    exit 1
fi

if [[ ! -f "${TOOLCHAIN_FILE}" ]]; then
    echo "ERROR: Toolchain file not found: ${TOOLCHAIN_FILE}"
    exit 1
fi

mkdir -p "${OPENCV_DEV_DIR}"


if [[ ! -d "${OPENCV_SRC_DIR}/.git" ]]; then
    git clone https://github.com/opencv/opencv.git "${OPENCV_SRC_DIR}"
fi

cd "${OPENCV_SRC_DIR}"

echo "===> Checking out ${OPENCV_BRANCH}..."
git fetch --tags --prune
git checkout "${OPENCV_BRANCH}"

echo "===> Cleaning old OpenCV build/install..."
rm -rf "${BUILD_DIR}" "${OPENCV_INSTALL_DIR}"
mkdir -p "${BUILD_DIR}" "${OPENCV_INSTALL_DIR}"

echo "===> Configuring OpenCV..."

cmake -S "${OPENCV_SRC_DIR}" -B "${BUILD_DIR}" \
  -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DCMAKE_SYSROOT="${SYSROOT}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${OPENCV_INSTALL_DIR}" \
  -DCMAKE_INSTALL_RPATH='$ORIGIN' \
  -DCMAKE_BUILD_WITH_INSTALL_RPATH=ON \
  -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=FALSE \
  -DBUILD_LIST=core,imgproc,imgcodecs,videoio,highgui,video \
  -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTS=OFF \
  -DBUILD_PERF_TESTS=OFF \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_opencv_python3=OFF \
  -DBUILD_opencv_python2=OFF \
  -DWITH_PYTHON=OFF \
  -DWITH_GSTREAMER=ON \
  -DWITH_V4L=ON \
  -DWITH_FFMPEG=ON \
  -DWITH_QT=OFF \
  -DWITH_GTK=OFF \
  -DWITH_IPP=OFF \
  -DWITH_OPENCL=OFF \
  -DVIDEOIO_ENABLE_PLUGINS=OFF \
  -DPNG_ARM_NEON=OFF \
  -DCPU_DISPATCH=

echo "===> Building OpenCV..."
cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "===> Installing OpenCV into ${OPENCV_INSTALL_DIR}"
cmake --install "${BUILD_DIR}"

echo "===> Done."