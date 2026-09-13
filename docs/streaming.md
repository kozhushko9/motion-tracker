# RTSP Streaming Guide

The application can optionally publish annotated camera frames through RTSP (Real Time Streaming Protocol) using GStreamer and MediaMTX.

The Raspberry Pi Zero 2W runs both:

* the MediaMTX RTSP server
* the `motion_tracker` application

An external computer, such as the Ubuntu development machine, connects to MediaMTX over the network to view the stream.

## Architecture

```text
Camera
  │
  ▼
motion_tracker
  │
  │ annotated frames
  ▼
GStreamer
  │
  │ H.264
  ▼
rtspclientsink
  │
  │ RTSP publish
  ▼
MediaMTX
127.0.0.1:8554
  │
  │ network
  ▼
Ubuntu host / RTSP viewer
```

The detection pipeline and RTSP streaming pipeline are asynchronous. Detection/tracking can be slower than camera capture, so the displayed bounding boxes/tracks may represent the latest available tracker state rather than the exact state for the displayed camera frame.

This is a **latest-result overlay** design rather than exact per-frame synchronized rendering.

---

## 1. Install MediaMTX on Raspberry Pi

MediaMTX must be installed on the **Raspberry Pi Zero 2W**.

The Zero 2W uses the ARM64 (`aarch64`) architecture when running Raspberry Pi OS 64-bit.

Check the architecture:

```bash
uname -m
```

Expected output:

```text
aarch64
```

### Download MediaMTX

This project uses MediaMTX `v1.21.0`.

Create a directory for MediaMTX:

```bash
mkdir -p ~/mediamtx
cd ~/mediamtx
```

Download the Linux ARM64 release directly from GitHub:

```bash
wget https://github.com/bluenviron/mediamtx/releases/download/v1.21.0/mediamtx_v1.21.0_linux_arm64.tar.gz
```

Extract the archive:

```bash
tar -xzf mediamtx_v1.21.0_linux_arm64.tar.gz
```

The directory should now contain:

```text
~/mediamtx/
├── mediamtx
└── mediamtx.yml
```

Verify the installation:

```bash
./mediamtx --version
```

You can optionally remove the downloaded archive:

```bash
rm mediamtx_v1.21.0_linux_arm64.tar.gz
```

---

## 2. Start MediaMTX

Before executing the main application binary on Raspberry Pi, start MediaMTX manually:

```bash
cd ~/mediamtx
./mediamtx
```

MediaMTX starts an RTSP server and listens on port `8554` by default.

Keep this terminal running.

In another SSH session, verify that port `8554` is listening:

```bash
ss -lntp | grep 8554
```

You should see a listening socket associated with MediaMTX.

---

## 3. Configure the application

Enable RTSP in `config.yaml`:

```text
enable_rtsp: true
rtsp_url: rtsp://127.0.0.1:8554/stream
```

The application uses GStreamer's `rtspclientsink` to publish the encoded video to MediaMTX.

### What does `127.0.0.1:8554` mean?

The URL `rtsp://127.0.0.1:8554/stream` does not come from camera. It is the address of the MediaMTX server running on Raspberry Pi.

```text
rtsp://127.0.0.1:8554/stream
       │          │      │
       │          │      └── MediaMTX path
       │          └───────── RTSP port
       └──────────────────── Raspberry Pi itself
```

* `rtsp://` — RTSP protocol.
* `127.0.0.1` — localhost, meaning the same Raspberry Pi.
* `8554` — MediaMTX RTSP port.
* `/stream` — the MediaMTX stream path.

Therefore:

```text
motion_tracker
      │
      │ publish
      ▼
rtsp://127.0.0.1:8554/stream
      │
      ▼
   MediaMTX
```

The `/stream` path is defined by the URL used by the publisher.

---

## 4. Start the application

MediaMTX must be running before the application attempts to publish the RTSP stream.

### Terminal 1 — MediaMTX

```bash
cd ~/mediamtx
./mediamtx
```

### Terminal 2 — motion_tracker

Start the application using the normal project startup command:

```bash
./run.sh
```

The application should then connect to:

```text
rtsp://127.0.0.1:8554/stream
```

and publish the H.264 stream.

---

## 5. Find the Raspberry Pi IP address

`127.0.0.1` only works **inside the Raspberry Pi**.

To connect from another computer, find the Raspberry Pi's network IP:

```bash
hostname -I
```

For example:

```text
192.168.1.18
```

The RTSP stream can then be accessed from another computer using:

```text
rtsp://192.168.1.18:8554/stream
```

Replace `192.168.1.18` with the actual IP address of the Raspberry Pi.

The distinction is:


Inside Raspberry Pi:

```text
rtsp://127.0.0.1:8554/stream
```

From another computer:

```text
rtsp://<pi-ip>:8554/stream
```

---

## 6. View the stream from Ubuntu

For low-latency playback, `ffplay` can be used:

```bash
ffplay -fflags nobuffer -flags low_delay rtsp://<pi-ip>:8554/stream
```

For example:

```bash
ffplay -fflags nobuffer -flags low_delay rtsp://192.168.1.18:8554/stream
```

You can also use VLC or another RTSP-compatible player.

---

## 7. Test the RTSP server independently

If the application does not work, first verify that MediaMTX itself is working.

MediaMTX should be running:

```bash
ps aux | grep mediamtx
```

and port `8554` should be listening:

```bash
ss -lntp | grep 8554
```

Then check the MediaMTX logs.

If MediaMTX is running correctly but the application cannot publish the stream, the problem is likely in the application's GStreamer/`rtspclientsink` pipeline rather than MediaMTX installation.

---

## 8. Important: `MediaMTX` must be running before `motion_tracker`

When RTSP is enabled:

```yaml
enable_rtsp: true
```

the application expects a MediaMTX server to be available at:

```text
rtsp://127.0.0.1:8554/stream
```

Therefore, the recommended startup order is:

```text
1. Start MediaMTX
        ↓
2. Start motion_tracker
        ↓
3. motion_tracker publishes RTSP stream
        ↓
4. Connect from Ubuntu / another RTSP client
```

If MediaMTX is not running, `rtspclientsink` cannot establish the RTSP connection.

Depending on the GStreamer pipeline and connection state, this can cause the streaming thread to block or otherwise interfere with the application's runtime behavior.

---

## 9. Run MediaMTX automatically at boot

For normal deployment, MediaMTX should eventually be configured as a `systemd` service so that it starts automatically when the Raspberry Pi boots.

The manual procedure above is recommended first because it makes troubleshooting easier.

Once the RTSP pipeline is confirmed to work, MediaMTX can be configured as a system service.

---

## Quick Start

After MediaMTX has been installed, the minimum procedure is:

### Raspberry Pi — Terminal 1

```bash
cd ~/mediamtx
./mediamtx
```

### Raspberry Pi — Terminal 2

```bash
cd ~/motion-tracker
./run.sh
```

### Raspberry Pi — find IP

```bash
hostname -I
```

### Ubuntu host

```bash
ffplay -fflags nobuffer -flags low_delay rtsp://<pi-ip>:8554/stream
```

For example:

```bash
ffplay -fflags nobuffer -flags low_delay rtsp://192.168.1.18:8554/stream
```
