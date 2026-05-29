#pragma once
#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <string>
#include <vector>

#include "config.hpp"
#include "fps_counter.hpp"
#include "frame.hpp"
#include "lockfree_ring_buffer.hpp"
#include "sort_tracker.hpp"

class Pipeline {
   public:
    explicit Pipeline(const AppConfig& cfg);

    void run();

    std::atomic<bool> running{true};

   private:
    static constexpr size_t kCacheLineSize = 64;

    AppConfig config_;

    std::string make_camera_pipeline() const;
    std::string make_output_pipeline() const;

    LockFreeRingBuffer<Frame> capture_queue_;

    std::map<int, std::string> class_names_;

    // Motion -> Detector
    alignas(kCacheLineSize) std::mutex detector_mtx_;
    std::condition_variable detector_condition_;
    std::shared_ptr<Frame> latest_frame_for_detector_;

    // Capture -> Stream
    alignas(kCacheLineSize) std::mutex stream_frame_mtx_;
    Frame latest_frame_for_stream_;
    bool has_stream_frame_ = false;

    // Detector -> Stream
    alignas(kCacheLineSize) std::mutex tracks_mtx_;
    std::vector<Track> latest_tracks_;

    FPSCounter cap_fps_;

    cv::VideoWriter video_writer_;
    bool video_writer_opened_ = false;

    void capture_loop();
    void motion_loop();
    void detect_loop();
    void stream_loop();
    void metrics_loop();

    void show_loop();
    void close_outputs();
};