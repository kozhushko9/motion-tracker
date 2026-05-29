#!/usr/bin/env bash
set -e

sudo apt update

sudo apt install -y \
    libcamera-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libyaml-cpp-dev \
    libglib2.0-dev \
    libpng-dev \
    gstreamer1.0-libcamera \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-rtsp \
    gstreamer1.0-tools \
    time \
    wget \
    rsync \
    gdbserver