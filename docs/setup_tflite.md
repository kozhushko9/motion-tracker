# TensorFlow Lite Setup

This project uses TensorFlow Lite C++ for running `.tflite` object detection models.

The project does not build or link the full TensorFlow framework at runtime.
Instead, it vendors only the minimal TensorFlow Lite runtime components consisting of:

- `libtensorflowlite.so`
- TensorFlow Lite headers
- FlatBuffers headers
- `benchmark_model`

The `./build_tflite.sh` script:

- Clones TensorFlow source code
- Configures TensorFlow build
- Builds TensorFlow Lite for ARM64
- Builds `benchmark_model`
- Vendors the runtime into `third_party/tflite/aarch64-linux-gnu`

### Vendored Runtime Layout

```text
third_party/tflite/
└── aarch64-linux-gnu/
    ├── bin/
    │   └── benchmark_model/
    ├── include/
    │   ├── tensorflow/
    │   └── flatbuffers/
    └── lib/
        └── libtensorflowlite.so
```

### Install Host Dependencies

```bash
sudo chmod +x ./scripts/*
./scripts/setup_host.sh
```

## Install Bazelisk as `bazel`

Bazel is required when building TensorFlow Lite from source or building TensorFlow Lite tools such as benchmark utilities.

This project uses Bazelisk (the recommended Bazel launcher), which automatically downloads and runs the correct Bazel version required by TensorFlow.

Install Bazelisk systemwide as bazel:

```bash
sudo wget -O /usr/local/bin/bazel https://github.com/bazelbuild/bazelisk/releases/download/v1.29.0/bazelisk-linux-amd64
sudo chmod +x /usr/local/bin/bazel
```

### Cross-Compile for Raspberry Pi (ARM64)

Build TensorFlow Lite:

```bash
./scripts/build_tflite.sh
```

The script builds ARM64 TensorFlow Lite binaries using Bazel cross-compilation and vendors them into:

```text
third_party/tflite/aarch64-linux-gnu
```

### Default Configuration

By default:

```bash
TFLITE_DEV_DIR=~/external_builds/TFLite
TF_BRANCH=v2.18.1
```

Both variables can be overridden:

```bash
TFLITE_DEV_DIR="/path/to/build_dir" \
TF_BRANCH=v2.21.0 \
./scripts/build_tflite.sh
```

### TensorFlow Configuration

During the first build, TensorFlow runs:

```bash
./configure
```

Recommended answers for Raspberry Pi CPU-only TensorFlow Lite builds:

| Question       | Recommended |
| -------------- | ----------- |
| ROCm support   | N           |
| CUDA support   | N           |
| Clang compiler | N           |

The project uses:

- CPU-only TensorFlow Lite
- ARM64
- XNNPACK delegate
- ARM NEON optimizations

### TensorFlow Lite Benchmark Tool

TensorFlow Lite provides a benchmarking utility called `benchmark_model`.

Example usage:

```bash
./benchmark_model \
    --graph=model.tflite \
    --num_threads=4
```

The tool reports:
- average inference latency
- memory usage
- etc.

See [benchmark_guide.md](./benchmark_guide.md) for the full benchmarking workflow.

### External Builds Cleanup

The TensorFlow source tree is only needed during the build process.

After vendoring the runtime into `third_party/tflite/`, the external build directory can be safely removed.

```bash
rm -rf ~/external_builds/TFLite
```

This removes temporary TensorFlow source/build files while preserving the vendored runtime inside the project.