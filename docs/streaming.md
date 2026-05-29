# RTSP Streaming Guide

RTSP shows the latest camera frame, then overlays the latest available tracker state on top of it. Because detection/tracking is slower than capture/streaming, the boxes/tracks can be slightly delayed relative to the image. 

Current design is a “latest-result overlay” design, not exact per-frame synchronized rendering.

## Architecture

This project streams annotated detection frames using GStreamer and MediaMTX.

```text
motion_tracker
    ↓
GStreamer appsrc
    ↓
H.264 encoder
    ↓
rtspclientsink
    ↓
MediaMTX RTSP server
    ↓
RTSP viewer
```

## Start the RTSP Server

Before executing the main application binary on Raspberry Pi, launch the MediaMTX server on host machine:

```bash
mediamtx
```

The application publishes its stream locally to:

```text
rtsp://127.0.0.1:8554/stream
```
## Connect External Clients

External clients can access the live stream from the Raspberry Pi using its network IP address:

```text
rtsp://<pi-ip>:8554/stream
```

## View the Stream

To watch the stream with minimal latency, use ffplay with low-delay flags enabled:

```text
ffplay -fflags nobuffer -flags low_delay rtsp://<pi-ip>:8554/stream
```