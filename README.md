# Motion-Triggered Object Detection Pipeline

Real-time, motion-triggered, low-latency object detection pipeline for Raspberry Pi ARM64 systems, built with GStreamer, TensorFlow Lite, XNNPACK delegate, OpenCV, and multithreaded C++20.

The project uses an Ubuntu host machine for ARM64 cross-compilation and deployment to Raspberry Pi Zero 2W.

## Development workflow

| Machine        | Role                                      |
| -------------- | ----------------------------------------- |
| Ubuntu Host    | cross-compilation / deployment            |
| Raspberry Pi   | runtime target                            |

The Ubuntu host:

- synchronizes Raspberry Pi sysroot
- cross-compiles ARM64 binaries
- builds vendored ARM64 OpenCV
- builds vendored ARM64 TensorFlow Lite
- deploys runtime libraries and binaries
- remotely launches the application

## Repository Structure

```
motion-tracker/
├── apps/                  # entry points
├── benchmarks/            # example .csv with .ipynb for analysis
├── cmake/                 # toolchains and dependency config
├── core/                  # pipeline orchestration
├── docs/                  # project documentation
├── models/                # used .tflite model(s)
├── modules/
│   ├── motion/            # motion detection
│   ├── detector/          # TensorFlow Lite inference
│   └── tracker/           # tracking logic
├── scripts/               # build + deployment scripts
├── third_party/           # vendored TFLite / OpenCV
├── utils/                 # utilities
│   ├── config/
│   ├── containers/
│   ├── metrics/
│   ├── paths/
│   ├── strings/
│   └── vision/
└── README.md
```

## Features

- Real-time camera capture (GStreamer + V4L2/libcamera)
- Motion-triggered inference pipeline
- TensorFlow Lite backend
- XNNPACK delegate acceleration (multi-threaded NEON kernels on ARM)
- ARM64 cross-compilation workflow
- Multithreaded producer-consumer pipeline
- Lock-free frame queue
- Runtime metrics and profiling
- TensorFlow Lite benchmarking
- RTP/H264 network streaming output
- Reproducible builds (TFLite via Bazel, OpenCV via CMake)

## Future Improvements 

- TensorFlow Lite GPU / delegate support
- DeepSORT integration
- DMA buffer zero-copy pipeline

## Tech Stack

| Component         | Technology                         |
| ----------------- | ---------------------------------- |
| Language          | C++20                              |
| Build System      | CMake + Ninja                      |
| Inference         | TensorFlow Lite / XNNPACK delegate |
| Vision            | OpenCV                             |
| Video Pipeline    | GStreamer                          |
| Camera Backend    | libcamera                          |
| Target Platform   | Raspberry Pi ARM64                 |
| Cross Compilation | aarch64 sysroot toolchain          |
| Benchmarking      | TensorFlow Lite benchmark_model    |
| Build Tools       | Bazelisk / Bazel                   |


## Pipeline Architecture

```
capture_loop
    ↓
capture_queue_ (lock-free ring buffer)
    ↓
motion_loop
    ↓
motion-triggered frames
    ↓
detect_loop
    ↓
TensorFlow Lite inference
    ↓
RTP/H264 streaming / metrics (FPS, latency, queue depth, etc.)
```

## Camera Pipeline

The Raspberry Pi camera stack uses:

- libcamera
- GStreamer
- OpenCV VideoCapture with CAP_GSTREAMER

This allows:

- low-overhead camera access
- explicit control of resolution and framerate
- compatibility with Raspberry Pi libcamera stack

## Threading Model

- capture_loop
    - continuously grabs frames from camera
    - pushes frames into lock-free queue
- motion_loop
    - polls capture queue
    - performs lightweight motion detection
    - forwards only motion-triggered frames
- detect_loop
    - waits efficiently on condition variable
    - runs TensorFlow Lite inference
    - performs postprocessing
    - overlays tracking visualization
    - pushes annotated frames to RTP/H264 stream
- stream_loop
    - receives annotated frames from detect_loop
    - performs asynchronous RTP/H264 streaming
    - isolates network/video output from inference pipeline
    - prevents encoder/network stalls from blocking detection
- metrics_loop
    - periodically reports stats

## Benchmark Results

Benchmark tool - TensorFlow Lite `benchmark_model`:

- Raspberry Pi Zero 2W
- 224x224 input
- Cortex-A53 CPU-only inference


| Model	          | Threads	| Avg Latency (ms) | FPS   | Memory Footprint (MB) |
| --------------- | ------- | ---------------- | ----- | --------------------- |
| yolo26n_float16 | 1       |	197.96         | 5.05  |  42.62                |
| yolo26n_float16 | 2       |	118.42	       | 8.44  |  42.90                |
| yolo26n_float16 | 4       |	86.68	       | 11.54 |  42.73                |
| yolo26n_float32 | 1       |	196.99	       | 5.08  |  36.09                |
| yolo26n_float32 | 2       |	119.57	       | 8.36  |  36.18                |
| yolo26n_float32 | 4       |	86.73	       | 11.53 |  35.97                |
| yolo26n_int8    |	1       |	222.12	       | 4.50  |  22.01                |
| yolo26n_int8    |	2       |	133.57	       | 7.49  |  21.77                |
| yolo26n_int8    |	4       |	99.73	       | 10.03 |  21.70                |

> **Note:** On Raspberry Pi ARMv8, enabling the TensorFlow Lite XNNPACK delegate reduced inference latency and improved FPS compared to the default interpreter.

> **Note:** Final application FPS is lower because preprocessing, motion detection, queues, visualization, and postprocessing also consume CPU time.



## Quick Start


Place your model files in  `./models`

Expected example:

`./models/yolo26n_float16.tflite`

Update the model name inside the configuration file:

```text
utils/config/config.yaml
```

Important configuration sections:

- camera
    - GStreamer camera pipeline
    - resolution
    - framerate
- model
    - TensorFlow Lite model name
- output
    - RTP/H264 streaming
    - encoder settings
     -bitrate
- metrics
    - runtime profiling
    - CSV logging

### First-Time Raspberry Pi Setup

Run once on the Ubuntu host:

```bash 
chmod +x ./scripts/*
./scripts/bootstrap_pi.sh <PI_HOST> [motion-tracker] [Release]
```
This performs:

- Host dependency setup
- Raspberry Pi dependency setup
- sysroot synchronization
- ARM64 OpenCV build
- ARM64 TensorFlow Lite build
- project cross-compilation
- deployment to Raspberry Pi


### Normal Rebuild Workflow

```bash
./scripts/build_pi.sh motion-tracker [Release]
./scripts/deploy_pi_binary.sh <PI_HOST> motion-tracker
```

### Full Redeploy

```bash
./scripts/build_pi.sh motion-tracker [Release]
./scripts/deploy_pi_full.sh <PI_HOST> motion-tracker
```

### Run on Raspberry Pi

```bash
ssh <PI_HOST>
cd /home/pi/motion-tracker
./run.sh
```


## Documentation

- [docs/setup_tflite.md](docs/setup_tflite.md) — Building and vendoring TensorFlow Lite for ARM64.
- [docs/benchmark_guide.md](docs/benchmark_guide.md) — Benchmark workflow and analysis.
- [docs/cross_compile.md](docs/cross_compile.md) — Raspberry Pi ARM64 cross-compilation workflow.
- [docs/streaming.md](docs/streaming.md) — RTSP streaming through MediaMTX.
