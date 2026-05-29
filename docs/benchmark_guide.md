# Benchmark Analysis Guide

This guide explains how to benchmark TensorFlow Lite models on Raspberry Pi using the TensorFlow Lite `benchmark_model` tool and analyze the generated results using `benchmark_results_analysis.ipynb`.

The benchmark measures **model-only inference performance**.

It does not include:

- camera capture
- preprocessing
- motion detection
- postprocessing
- tracking
- visualization
- RTP/H264 streaming
- thread synchronization
- queue overhead

---

### Benchmark Tool Location

The TensorFlow Lite benchmark tool is deployed to:

```
/home/pi/<target_dir>/tools/benchmark_model
```

Example:

```
/home/pi/motion-tracker/tools/benchmark_model
```

The binary is built from TensorFlow Lite source and vendored into:

```
third_party/tflite/aarch64-linux-gnu/bin/benchmark_model
```

### Preparing Benchmark Workspace

On Raspberry Pi:

```bash
mkdir -p ~/benchmark_yolo/models
mkdir -p ~/benchmark_yolo/results
```

### Add TensorFlow Lite Models

Place exported `.tflite` models into:

```text
~/benchmark_yolo/models/
```

Example:

```text
yolo26n_float32.tflite
yolo26n_float16.tflite
yolo26n_int8.tflite
```

### Configure Thread Counts

The benchmark script uses `THREADS_LIST` to evaluate different CPU thread counts.

In benchmark testing, threads are almost always scaled by powers of two (for architectural reasons). 

Check available CPU cores:

```bash
nproc
```

For Raspberry Pi Zero 2W:

```text
4 Cortex-A53 cores
```

Typical thread counts:

```text
1 2 4
```

Avoid using more threads than physical CPU cores.

### Run benchmarks

On Raspberry Pi:

```bash
cd ~/benchmark_yolo

BENCHMARK_BIN=~/motion-tracker/tools/benchmark_model \
THREADS_LIST="1 2 4" \
MODEL_DIR=~/benchmark_yolo/models \
./run_tflite_benchmarks.sh
```

If the deployment directory is different:

```bash
BENCHMARK_BIN=/home/pi/<target_dir>/tools/benchmark_model
```

The generated CSV file is stored in:

```text
~/benchmark_yolo/results/aarch64_benchmark_results.csv
```

## Benchmark Data Structure

The notebook `benchmark_results_analysis.ipynb` reads performance metrics from a CSV file.

The CSV tracks :

| Column       | Description                                   |
| ------------ | --------------------------------------------- |
| `model`      | `.tflite` model filename                      |
| `threads`    | Number of CPU threads used                    |
| `avg_ms`     | Average inference latency in milliseconds     |
| `fps`        | Approximate inference-only frames per second  |
| `max_rss_mb` | Peak memory usage in megabytes                |


Final application FPS will be lower because the full pipeline also includes:

- camera capture
- preprocessing
- postprocessing
- motion detection
- tracking
- drawing
- video streaming
- thread synchronization and queues

---

## Python Environment Setup

### Create a Virtual Environment

From the project root on Ubuntu:

```bash
python3 -m venv .venv
source .venv/bin/activate
```

### Install Dependencies

Install the required data visualization and kernel packages:

```bash
pip install -r requirements.txt
```
---

## Running the Notebook in VS Code

1. Open this project folder in **Visual Studio Code**.
2. Install the **Python** and **Jupyter** extensions.
3. Open `benchmark_results_analysis.ipynb`.
4. Click **Select Kernel** in the top-right corner of the notebook editor.
5. Select **Python Environments...** and choose the workspace environment labeled **`.venv`**.
6. Run all cells.

## Expected Output

The notebook generates charts comparing:

- average latency in milliseconds
- approximate FPS
- memory footprint in MB

It also prints the best-performing model/thread configuration.

## Test Platform

Benchmarks in this repository were collected on:

- Raspberry Pi Zero 2W
- ARM Cortex-A53 (4 cores)
- TensorFlow Lite XNNPACK delegate
- ARM64 (aarch64)
