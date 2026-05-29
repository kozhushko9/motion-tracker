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

LOCAL_BIN="${PROJECT_ROOT}/install/${TARGET_DIR}/usr/bin/motion_tracker"
PI_PATH="/home/pi/${TARGET_DIR}"

if [[ ! -x "${LOCAL_BIN}" ]]; then
    echo "ERROR: Missing binary:"
    echo "  ${LOCAL_BIN}"
    echo "Run:"
    echo "  ./scripts/build_pi.sh ${TARGET_DIR} Release"
    exit 1
fi

echo "===> Deploying only binary"

ssh "${PI_HOST}" "mkdir -p '${PI_PATH}/bin'"

rsync -avz \
    "${LOCAL_BIN}" \
    "${PI_HOST}:${PI_PATH}/bin/motion_tracker"

echo "===> Binary deploy complete"
echo "Run:"
echo "  ssh ${PI_HOST} 'cd ${PI_PATH} && ./run.sh'"