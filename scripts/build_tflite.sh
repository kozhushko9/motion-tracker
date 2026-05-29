#!/usr/bin/env bash
set -euo pipefail

TF_BRANCH="${TF_BRANCH:-v2.18.1}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

TFLITE_DEV_DIR="${TFLITE_DEV_DIR:-$HOME/external_builds/TFLite}"
TF_SRC_DIR="${TF_SRC_DIR:-${TFLITE_DEV_DIR}/tensorflow_src}"

VENDOR_DIR="${PROJECT_ROOT}/third_party/tflite/aarch64-linux-gnu"
VENDOR_INCLUDE_DIR="${VENDOR_DIR}/include"
VENDOR_LIB_DIR="${VENDOR_DIR}/lib"
VENDOR_BIN_DIR="${VENDOR_DIR}/bin"

echo "===> Building TensorFlow Lite for Raspberry Pi / aarch64"
echo "===> TensorFlow source: ${TF_SRC_DIR}"
echo "===> TensorFlow branch: ${TF_BRANCH}"
echo "===> Vendor dir: ${VENDOR_DIR}"

mkdir -p "${TFLITE_DEV_DIR}"

if [[ ! -d "${TF_SRC_DIR}/.git" ]]; then
    git clone https://github.com/tensorflow/tensorflow.git "${TF_SRC_DIR}"
fi

cd "${TF_SRC_DIR}"

echo "===> Checking out ${TF_BRANCH}..."
git fetch --tags --prune
git checkout "${TF_BRANCH}"

if [ ! -f ".tf_configure.bazelrc" ]; then
    echo "===> Running TensorFlow configure..."
    ./configure
else
    echo "===> TensorFlow already configured. Skipping ./configure"
    echo "     Delete ${TF_SRC_DIR}/.tf_configure.bazelrc if you want to reconfigure."
fi

echo "===> Building libtensorflowlite.so and benchmark_model for aarch64..."


bazel build -c opt \
    --config=elinux_aarch64 \
    //tensorflow/lite:libtensorflowlite.so \
    //tensorflow/lite/tools/benchmark:benchmark_model

BAZEL_BIN="$(bazel info bazel-bin --config=elinux_aarch64)"

TFLITE_SO="${BAZEL_BIN}/tensorflow/lite/libtensorflowlite.so"
BENCHMARK_MODEL="${BAZEL_BIN}/tensorflow/lite/tools/benchmark/benchmark_model"

test -f "${TFLITE_SO}"
test -f "${BENCHMARK_MODEL}"

FLATBUFFERS_INCLUDE_DIRS="$(find "$HOME/.cache/bazel" \
    -path "*/external/flatbuffers/include/flatbuffers/base.h" \
    2>/dev/null \
    | sed 's#/flatbuffers/base.h##')"

if [ -z "${FLATBUFFERS_INCLUDE_DIRS}" ]; then
    echo "ERROR: Could not find Bazel FlatBuffers include dir."
    exit 1
fi

for dir in ${FLATBUFFERS_INCLUDE_DIRS}; do
    major="$(grep 'FLATBUFFERS_VERSION_MAJOR' "$dir/flatbuffers/base.h" | awk '{print $3}')"

    if [ "$major" = "24" ]; then
        FLATBUFFERS_INCLUDE_DIR="$dir"
        FLATBUFFERS_MAJOR="$major"
        break
    fi
done

if [ -z "${FLATBUFFERS_INCLUDE_DIR:-}" ]; then
    echo "ERROR: No FlatBuffers v24 found in Bazel cache."
    exit 1
fi

echo "===> FlatBuffers include dir: ${FLATBUFFERS_INCLUDE_DIR}"
echo "===> FlatBuffers major version: ${FLATBUFFERS_MAJOR}"


echo "===> Cleaning old vendored TFLite files..."
rm -rf "${VENDOR_DIR}"

mkdir -p "${VENDOR_INCLUDE_DIR}/tensorflow"
mkdir -p "${VENDOR_INCLUDE_DIR}/tensorflow/compiler/mlir"
mkdir -p "${VENDOR_LIB_DIR}"
mkdir -p "${VENDOR_BIN_DIR}"

echo "===> Copying libtensorflowlite.so..."
rsync -a "${TFLITE_SO}" "${VENDOR_LIB_DIR}/"

echo "===> Copying TensorFlow Lite headers..."
rsync -a "${TF_SRC_DIR}/tensorflow/lite" \
    "${VENDOR_INCLUDE_DIR}/tensorflow/"

echo "===> Copying TensorFlow compiler Lite headers..."
rsync -a "${TF_SRC_DIR}/tensorflow/compiler/mlir/lite" \
    "${VENDOR_INCLUDE_DIR}/tensorflow/compiler/mlir/"

echo "===> Copying FlatBuffers headers..."
rsync -a "${FLATBUFFERS_INCLUDE_DIR}/flatbuffers" \
    "${VENDOR_INCLUDE_DIR}/"

echo "===> Copying benchmark_model..."
rsync -a "${BENCHMARK_MODEL}" "${VENDOR_BIN_DIR}/"
chmod +x "${VENDOR_BIN_DIR}/benchmark_model"


echo "===> Verifying vendored files..."

test -f "${VENDOR_LIB_DIR}/libtensorflowlite.so"
test -f "${VENDOR_INCLUDE_DIR}/tensorflow/lite/interpreter.h"
test -f "${VENDOR_INCLUDE_DIR}/tensorflow/lite/kernels/register.h"
test -f "${VENDOR_INCLUDE_DIR}/tensorflow/lite/model.h"
test -f "${VENDOR_INCLUDE_DIR}/tensorflow/lite/delegates/xnnpack/xnnpack_delegate.h"
test -f "${VENDOR_INCLUDE_DIR}/flatbuffers/flatbuffers.h"
test -x "${VENDOR_BIN_DIR}/benchmark_model"

echo "===> Library architecture:"
file "${VENDOR_LIB_DIR}/libtensorflowlite.so"

echo "===> benchmark_model architecture:"
file "${VENDOR_BIN_DIR}/benchmark_model"

echo "===> Done."
echo "TensorFlow Lite was vendored into:"
echo "  ${VENDOR_DIR}"