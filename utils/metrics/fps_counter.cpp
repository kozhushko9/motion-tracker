#include "fps_counter.hpp"

FPSCounter::FPSCounter()
    : frame_count_(0),
      fps_(0.0),
      last_time_(clock::now()) {}

void FPSCounter::tick() {
    frame_count_++;

    auto now = clock::now();
    auto elapsed = std::chrono::duration<double>(now - last_time_).count();

    // update every 1 second
    if (elapsed >= 1.0) {

        fps_.store(frame_count_ / elapsed);

        frame_count_ = 0;
        last_time_ = now;
    }
}

double FPSCounter::get_fps() const {
    return fps_.load();
}
