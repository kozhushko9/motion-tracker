#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]] ; then
    echo "Usage: $0 <pi_host> [target_dir] [build_type]"
    echo "Examples:"
    echo "  $0 raspberrypi.local"
    echo "  $0 192.168.1.50 motion_tracker Release"
    exit 1
fi

PI_HOST="$1"
TARGET_DIR="${2:-$(basename "$PWD")}"
BUILD_TYPE="${3:-Release}"

# Check for valid CMake build types
if [[ ! "$BUILD_TYPE" =~ ^(Debug|Release|RelWithDebInfo|MinSizeRel)$ ]]; then
    echo "Error: Invalid build type '$BUILD_TYPE'."
    echo "Allowed: Debug, Release, RelWithDebInfo, MinSizeRel"
    exit 1
fi

echo "===> Bootstrap Raspberry Pi build/deploy"
echo "===> Pi host:    $PI_HOST"
echo "===> Target dir: $TARGET_DIR"
echo "===> Build type: $BUILD_TYPE"

# 1. Setup host once/check host tools
if [[ -x ./scripts/setup_host.sh ]]; then
    echo "===> Checking host setup"
    ./scripts/setup_host.sh
fi

# 2. Setup Pi runtime/dev dependencies
echo "===> Setting up Raspberry Pi"
scp -p ./scripts/setup_pi.sh "${PI_HOST}:/home/pi/setup_pi.sh"
ssh "$PI_HOST" "chmod +x /home/pi/setup_pi.sh && /home/pi/setup_pi.sh && rm /home/pi/setup_pi.sh"

# 3. Sync sysroot from Pi (used as build environment)
echo "===> Syncing sysroot"
./scripts/sync_sysroot.sh "$PI_HOST"

# 4. Build third-party dependencies for Pi
echo "===> Building OpenCV for Pi"
./scripts/build_opencv.sh

echo "===> Building TensorFlow Lite for Pi"
./scripts/build_tflite.sh

# 5. Build (cross-compile) project for Pi
echo "===> Building project for Pi"
./scripts/build_pi.sh "$TARGET_DIR" "$BUILD_TYPE"

# 6. Full deploy
echo "===> Deploying full app to Pi"
./scripts/deploy_pi_full.sh "$PI_HOST" "$TARGET_DIR"

echo "===> Bootstrap complete"
echo "Run manually:"
echo "  ssh $PI_HOST 'cd /home/pi/$TARGET_DIR && ./run.sh'"