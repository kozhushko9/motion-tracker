#pragma once
#include <map>
#include <string>

struct CameraConfig {
    int width;
    int height;
    int capture_fps;
    std::string pipeline;
};

struct DetectorConfig {
    int num_threads;
    float conf_threshold;
    std::map<int, std::string> class_names;
};

struct QueueConfig {
    int capture_queue_capacity;
};

struct ModelConfig {
    std::string path;
};

struct MetricsConfig {
    bool enable_printing = true;
    std::string csv_path;
    int report_interval_s = 5;
};

struct OutputConfig {
    bool enable_rtsp;
    std::string rtsp_url;

    bool save_video;
    std::string video_path;

    int output_fps;
    int bitrate_kbps;

    bool use_hw_encoder;

    std::string pipeline;
};

struct AppConfig {
    CameraConfig camera;
    DetectorConfig detector;
    QueueConfig queue;
    ModelConfig model;
    MetricsConfig metrics;
    OutputConfig output;
};

AppConfig load_config(const std::string& path);