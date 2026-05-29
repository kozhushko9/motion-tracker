#pragma once
#include <atomic>
#include <chrono>

class FPSCounter {
public:
    FPSCounter();

    void tick();                  // call per frame
    double get_fps() const;       // FPS

private:
    using clock = std::chrono::steady_clock;

    std::atomic<uint64_t> frame_count_;
    std::atomic<double> fps_;

    clock::time_point last_time_;
};
