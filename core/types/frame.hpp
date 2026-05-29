#pragma once

#include <chrono>
#include <cstdint>
#include <opencv2/core.hpp>

struct Frame {
    cv::Mat image;

    // Monotonic timestamp created by application
    // Used for measuring pipeline latency
    std::chrono::steady_clock::time_point created_at;

    // Optional: sequential frame id for debugging/logging
    uint64_t id = 0;
};