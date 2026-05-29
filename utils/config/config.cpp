#include "config.hpp"

#include <yaml-cpp/yaml.h>

#include <iostream>

AppConfig load_config(const std::string& path) {
    YAML::Node config = YAML::LoadFile(path);

    AppConfig cfg;

    std::cerr << "[CONFIG] output node:\n"
              << config << "\n";

    cfg.camera.width = config["camera"]["width"].as<int>();
    cfg.camera.height = config["camera"]["height"].as<int>();
    cfg.camera.capture_fps = config["camera"]["capture_fps"].as<int>();
    cfg.camera.pipeline = config["camera"]["pipeline"].as<std::string>();

    cfg.detector.num_threads = config["detector"]["num_threads"].as<int>();
    cfg.detector.conf_threshold = config["detector"]["conf_threshold"].as<float>();

    // Parse the class_names map
    if (config["detector"]["class_names"]) {
        for (auto it = config["detector"]["class_names"].begin();
             it != config["detector"]["class_names"].end();
             ++it) {
            int id = it->first.as<int>();
            std::string name = it->second.as<std::string>();
            cfg.detector.class_names[id] = name;
        }
    }

    cfg.queue.capture_queue_capacity = config["queue"]["capture_queue_capacity"].as<int>();

    cfg.model.path = config["model"]["path"].as<std::string>();

    if (config["metrics"]) {
        cfg.metrics.enable_printing = config["metrics"]["enable_printing"].as<bool>(true);
        cfg.metrics.csv_path = config["metrics"]["csv_path"].as<std::string>("");
        cfg.metrics.report_interval_s = config["metrics"]["report_interval_s"].as<int>(5);
    }

    cfg.output.enable_rtsp = config["output"]["enable_rtsp"].as<bool>();
    cfg.output.rtsp_url = config["output"]["rtsp_url"].as<std::string>();
    cfg.output.save_video = config["output"]["save_video"].as<bool>();
    cfg.output.video_path = config["output"]["video_path"].as<std::string>();
    cfg.output.output_fps = config["output"]["output_fps"].as<int>();
    cfg.output.bitrate_kbps = config["output"]["bitrate_kbps"].as<int>();
    cfg.output.use_hw_encoder = config["output"]["use_hw_encoder"].as<bool>();

    cfg.output.pipeline = config["output"]["pipeline"].as<std::string>();

    return cfg;
}
