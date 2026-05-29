#!/usr/bin/env bash
set -euo pipefail

PI_HOST="$1"

if [ -z "$PI_HOST" ]; then
    echo "Usage: $0 <pi_host>"
    echo "Examples:"
    echo "raspberrypi.local"
    echo "192.168.1.50"
    exit 1
fi

# Ensure 'symlinks' utility is installed
if ! command -v symlinks > /dev/null; then
    echo "===> 'symlinks' utility not found. Installing..."
    sudo apt update && sudo apt install -y symlinks
fi

SYSROOT="$(realpath ./sysroot)"

echo "===> Reset sysroot"
rm -rf "$SYSROOT"
mkdir -p "$SYSROOT/usr/include" "$SYSROOT/usr/lib"

# Create the same symlink the Pi has so the compiler finds /lib
ln -s usr/lib "$SYSROOT/lib"

# Header files (.h) needed for compilation
echo "===> Syncing /usr/include"
rsync -avz --delete \
  --rsync-path="sudo rsync" \
  "${PI_HOST}:/usr/include/" \
  "$SYSROOT/usr/include/"

# Essential system libraries and dynamic linker
echo "===> Syncing /usr/lib (with exclusions)"
rsync -avz --delete \
  --rsync-path="sudo rsync" \
  --exclude='python*' \
  --exclude='apt' \
  --exclude='dpkg' \
  --exclude='modules' \
  --exclude='firmware' \
  "${PI_HOST}:/usr/lib/" \
  "$SYSROOT/usr/lib/"

# Convert absolute symlinks to relative links
echo "===> Fixing absolute symlinks"
symlinks -cr "$SYSROOT"

echo "===> Sysroot ready: $SYSROOT"