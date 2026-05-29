#!/usr/bin/env bash
set -euo pipefail

ARCH="$(uname -m)"

if [[ "$ARCH" != "aarch64" ]]; then
    echo "[ERROR] This benchmark script must run on Raspberry Pi / aarch64."
    echo "        Current architecture: $ARCH"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

TIME_BIN="${TIME_BIN:-/usr/bin/time}"

TFLITE_PLATFORM="aarch64-linux-gnu"

# Either user-provided BENCHMARK_BIN or found in PATH or project fallback
if [[ -n "${BENCHMARK_BIN:-}" ]]; then
    :
elif command -v benchmark_model >/dev/null 2>&1; then
    BENCHMARK_BIN="$(command -v benchmark_model)"
else
    BENCHMARK_BIN="$PROJECT_ROOT/third_party/tflite/$TFLITE_PLATFORM/bin/benchmark_model"
fi

MODEL_DIR="${MODEL_DIR:-$HOME/benchmark_yolo/models}"
RESULT_DIR="${RESULT_DIR:-$HOME/benchmark_yolo/results}"

mkdir -p "$RESULT_DIR"

RESULTS_TXT="$RESULT_DIR/${ARCH}_benchmark_results.txt"
RESULTS_CSV="$RESULT_DIR/${ARCH}_benchmark_results.csv"

if [[ ! -x "$BENCHMARK_BIN" ]]; then
    echo "[ERROR] benchmark_model not found or not executable:"
    echo "        $BENCHMARK_BIN"
    exit 1
fi

if [[ ! -d "$MODEL_DIR" ]]; then
    echo "[ERROR] Model directory not found:"
    echo "        $MODEL_DIR"
    exit 1
fi

# Prepare CSV
echo "model,threads,avg_ms,fps,max_rss_mb" > "$RESULTS_CSV"
: > "$RESULTS_TXT"

shopt -s nullglob
models=("$MODEL_DIR"/*.tflite)

if [[ ${#models[@]} -eq 0 ]]; then
    echo "[ERROR] No .tflite models found in:"
    echo "        $MODEL_DIR"
    exit 1
fi

THREADS_LIST="${THREADS_LIST:-1 2 4}"

for model in "${models[@]}"; do
    for threads in $THREADS_LIST; do
        model_name="$(basename "$model")"

        {
            echo "========================================="
            echo "MODEL: $model_name"
            echo "ARCH: $ARCH"
            echo "THREADS: $threads"
            echo "========================================="
        } | tee -a "$RESULTS_TXT"

        output=$("$TIME_BIN" -v "$BENCHMARK_BIN" \
            --graph="$model" \
            --num_threads="$threads" \
            --warmup_runs=5 \
            --num_runs=50 \
            2>&1)

        # Save raw benchmark_model output
        echo "$output" >> "$RESULTS_TXT"

        # Extract average inference time in microseconds
        avg_us=$(echo "$output" | sed -nE 's/.*Inference \(avg\):[[:space:]]*([0-9.]+).*/\1/p' | tail -n 1)

        max_rss_kb=$(echo "$output" | sed -nE 's/.*Maximum resident set size \(kbytes\):[[:space:]]*([0-9]+).*/\1/p' | tail -n 1)

        if [[ -z "$max_rss_kb" ]]; then
            max_rss_mb="N/A"
        else
            max_rss_mb=$(awk -v kb="$max_rss_kb" 'BEGIN { printf "%.2f", kb / 1024.0 }')
        fi

        if [[ -z "$avg_us" ]]; then
            echo "[WARN] Could not parse average inference time" | tee -a "$RESULTS_TXT"
            echo "Average inference: N/A" | tee -a "$RESULTS_TXT"
            echo "$model_name,$threads,N/A,N/A,$max_rss_mb" >> "$RESULTS_CSV"
            echo | tee -a "$RESULTS_TXT"
            continue
        fi

        avg_ms=$(awk -v us="$avg_us" 'BEGIN { printf "%.2f", us / 1000.0 }')
        fps=$(awk -v ms="$avg_ms" 'BEGIN { printf "%.2f", 1000.0 / ms }')

        {
            echo
            echo "Average inference: $avg_ms ms"
            echo "Approx FPS:        $fps"
            echo "Peak memory:       $max_rss_mb MB"
            echo
        } | tee -a "$RESULTS_TXT"

        echo "$model_name,$threads,$avg_ms,$fps,$max_rss_mb" >> "$RESULTS_CSV"
    done
done

echo "Saved raw log to: $RESULTS_TXT"
echo "Saved CSV table to: $RESULTS_CSV"