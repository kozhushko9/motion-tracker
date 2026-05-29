#!/usr/bin/env bash
set -e

sudo apt update

sudo apt install -y \
    build-essential \
    crossbuild-essential-arm64 \
    python3 \
    python3-dev \
    python3-pip \
    cmake \
    ninja-build \
    ccache \
    pkg-config \
    git \
    gdb \
    valgrind \
    zip \
    unzip \
    wget \
    curl \
    time \
    gdb-multiarch 
