#!/usr/bin/env bash
set -euo pipefail

PI_HOST="${1:-}"
TARGET_DIR="${2:-motion-tracker}"

if [[ -z "$PI_HOST" ]]; then
    echo "Usage: $0 <pi_host> [target_dir]"
    echo "Example: $0 zero2w motion-tracker"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

INSTALL_DIR="${PROJECT_ROOT}/install/${TARGET_DIR}"
TFLITE_DIR="${PROJECT_ROOT}/third_party/tflite/aarch64-linux-gnu"

PI_PATH="/home/pi/${TARGET_DIR}"

if [[ ! -d "${INSTALL_DIR}/usr" ]]; then
    echo "ERROR: Missing staged install:"
    echo "  ${INSTALL_DIR}/usr"
    echo "Run:"
    echo "  ./scripts/build_pi.sh ${TARGET_DIR} Release"
    exit 1
fi

echo "===> Full deploy to ${PI_HOST}:${PI_PATH}"

# ssh "${PI_HOST}" "mkdir -p '${PI_PATH}/lib' '${PI_PATH}/bin' '${PI_PATH}/tools'"






ssh "${PI_HOST}" "mkdir -p '${PI_PATH}'"

rsync -avz --delete \
  "$INSTALL_DIR/usr/" \
  "${PI_HOST}:${PI_PATH}/"

if [[ -x "${TFLITE_DIR}/bin/benchmark_model" ]]; then
    ssh "${PI_HOST}" "mkdir -p '${PI_PATH}/tools'"
    rsync -avz \
        "${TFLITE_DIR}/bin/benchmark_model" \
        "${PI_HOST}:${PI_PATH}/tools/"
fi

RUN_SCRIPT="$(mktemp)"

cat > "$RUN_SCRIPT" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export LD_LIBRARY_PATH="${APP_DIR}/lib:${APP_DIR}/lib/aarch64-linux-gnu:${LD_LIBRARY_PATH:-}"

TARGET_BIN="${APP_DIR}/bin/motion_tracker"

if [[ ! -x "${TARGET_BIN}" ]]; then
    echo "Error: executable not found:"
    echo "  ${TARGET_BIN}"
    exit 1
fi

echo "Starting ${TARGET_BIN}..."
exec "${TARGET_BIN}" "$@"
EOF

chmod +x "$RUN_SCRIPT"
rsync -avz "$RUN_SCRIPT" "${PI_HOST}:${PI_PATH}/run.sh"
rm -f "$RUN_SCRIPT"


echo "===> Full deploy complete"
echo "Run:"
echo "  ssh ${PI_HOST} 'cd ${PI_PATH} && ./run.sh'"