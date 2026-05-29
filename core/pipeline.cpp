#include "pipeline.hpp"

#include <chrono>
#include <csignal>
#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <thread>
#include <utility>

#include "detection_result.hpp"
#include "detector.hpp"
#include "draw_utils.hpp"
#include "metrics.hpp"
#include "motion.hpp"
#include "scoped_timer.hpp"
#include "sort_tracker.hpp"
#include "strings.hpp"

Pipeline::Pipeline(const AppConfig& cfg)
    : config_(cfg),
      capture_queue_(cfg.queue.capture_queue_capacity),
      class_names_(cfg.detector.class_names) {
    g_metrics.open_csv_log(config_.metrics);
}

Pipeline* g_pipeline = nullptr;

void signal_handler(int) {
    if (g_pipeline) {
        g_pipeline->running.store(false);
    }
}

std::string Pipeline::make_camera_pipeline() const {
    return utils::replace_tokens(
        config_.camera.pipeline,
        {
            {"{width}", std::to_string(config_.camera.width)},
            {"{height}", std::to_string(config_.camera.height)},
            {"{fps}", std::to_string(config_.camera.capture_fps)},
        });
}

std::string Pipeline::make_output_pipeline() const {
    std::string record_branch;

    if (config_.output.save_video) {
        record_branch =
            "t. ! queue ! "
            "matroskamux ! "
            "filesink location=" +
            config_.output.video_path + " sync=false";
    }

    return utils::replace_tokens(
        config_.output.pipeline,
        {
            {"{width}", std::to_string(config_.camera.width)},
            {"{height}", std::to_string(config_.camera.height)},
            {"{fps}", std::to_string(config_.output.output_fps)},
            {"{bitrate}", std::to_string(config_.output.bitrate_kbps)},
            {"{url}", config_.output.rtsp_url},
            {"{record_branch}", record_branch},
        });
}

void Pipeline::capture_loop() {
    const std::string pipeline = make_camera_pipeline();

    std::cerr << "[CAMERA] Using pipeline: " << pipeline << std::endl;

    cv::VideoCapture cap;

    auto open_camera = [&]() {
        for (int i = 0; i < 5; ++i) {
            std::cerr << "\n[CAMERA] Opening attempt\n";

            cap.release();
            cap.open(pipeline, cv::CAP_GSTREAMER);

            if (cap.isOpened()) {
                std::cerr << "\n[CAMERA] Opened successfully\n";

                // warmup (IMPORTANT for libcamera)
                cv::Mat tmp;
                for (int j = 0; j < 5; ++j) cap.read(tmp);

                return true;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        return false;
    };

    if (!open_camera()) {
        std::cerr << "\n[FATAL] Cannot open camera\n";
        running.store(false);
        return;
    }

    uint64_t next_frame_id = 0;
    Frame f;

    while (running.load()) {
        bool got_frame = false;

        {
            ScopedTimer capread_timer("cap.read");
            got_frame = cap.read(f.image);
        }

        if (!got_frame || f.image.empty()) {
            std::cerr << "\n[CAMERA] READ FAILED -> restarting...\n";

            g_metrics.increment("capture.read_failures");

            cap.release();
            std::this_thread::sleep_for(std::chrono::seconds(1));

            if (!open_camera()) {
                std::cerr << "\n[FATAL] Camera restart failed\n";
                running.store(false);
                break;
            }

            continue;
        }

        f.created_at = std::chrono::steady_clock::now();
        f.id = next_frame_id++;

        bool was_dropped = capture_queue_.push(f);  // drop if full

        {
            std::lock_guard<std::mutex> lock(stream_frame_mtx_);
            latest_frame_for_stream_ = f;  // shallow cv::Mat copy
            has_stream_frame_ = true;
        }

        cap_fps_.tick();  // measuring capture throughput

        g_metrics.increment("capture.frames");
        g_metrics.record("capture.queue.size", capture_queue_.size());

        if (was_dropped) {
            g_metrics.increment("capture.queue.dropped");
        }
    }

    std::cerr << "\n[CAMERA] Releasing camera...\n";
    cap.release();
}

void Pipeline::motion_loop() {
    Motion motion;
    Frame f;

    while (running.load()) {
        // capture_queue_ is non-blocking
        // If no frame is available right now, avoid busy-spinning
        if (!capture_queue_.pop(f)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        g_metrics.increment("motion.frames");

        bool has_motion = false;

        {
            ScopedTimer motion_timer("motion.detect");
            has_motion = motion.detect(f);
        }

        if (!has_motion) {
            g_metrics.increment("motion.not_triggered");
            continue;
        }

        g_metrics.increment("motion.triggered");

        // Moving f into new_frame doesn’t duplicate pixel data — it just transfers ownership of the header
        auto new_frame = std::make_shared<Frame>();
        *new_frame = std::move(f);

        // Protect shared detector input
        {
            std::lock_guard<std::mutex> lock(detector_mtx_);

            if (latest_frame_for_detector_) {
                g_metrics.increment("detector.input.overwritten");
            }

            latest_frame_for_detector_ = std::move(new_frame);
        }

        // Wake detector thread after the frame is ready
        detector_condition_.notify_one();
    }

    // Wake detector thread so it can exit if it is waiting
    detector_condition_.notify_all();
}

void Pipeline::detect_loop() {
    Detector detector(config_);
    SortTracker tracker;
    DetectionResult result;
    std::vector<Track> tracks;

    while (running.load()) {
        std::shared_ptr<Frame> frame_ptr;

        {
            std::unique_lock<std::mutex> lock(detector_mtx_);

            detector_condition_.wait(lock, [&]() {
                return !running.load() || latest_frame_for_detector_ != nullptr;
            });

            if (!running.load()) {
                break;
            }

            frame_ptr = std::move(latest_frame_for_detector_);
            latest_frame_for_detector_.reset();
        }

        if (!frame_ptr) {
            continue;
        }

        detector.detect(*frame_ptr, result);

        g_metrics.increment("detector.runs");

        if (!result.boxes.empty()) {
            g_metrics.increment("detector.positive_frames");
            g_metrics.increment("detector.objects", result.boxes.size());
        }

        {
            ScopedTimer update_timer("tracker.update");
            tracks = tracker.update(
                result.boxes,
                result.class_ids,
                result.scores,
                frame_ptr->created_at);
        }

        g_metrics.record("tracker.active_tracks", static_cast<double>(tracks.size()));

        size_t stable_tracks = 0;
        for (const Track& t : tracks) {
            if (t.hits >= 2) {
                stable_tracks++;
            }
        }

        g_metrics.record("tracker.stable_tracks", static_cast<double>(stable_tracks));

        {
            std::lock_guard<std::mutex> lock(tracks_mtx_);
            latest_tracks_ = tracks;
        }

        auto now = std::chrono::steady_clock::now();

        double latency_ms = std::chrono::duration<double, std::milli>(
                                now - frame_ptr->created_at)
                                .count();

        g_metrics.record("pipeline.latency", latency_ms);
    }
}

void Pipeline::stream_loop() {
    if (!config_.output.enable_rtsp) {
        return;
    }

    const int output_fps = std::max(1, config_.output.output_fps);
    const int frame_delay_ms = std::max(1, 1000 / output_fps);

    Frame frame_copy;
    std::vector<Track> tracks_copy;

    while (running.load()) {
        {
            std::lock_guard<std::mutex> lock(stream_frame_mtx_);

            if (!has_stream_frame_ || latest_frame_for_stream_.image.empty()) {
                // Avoid busy-spinning before the first frame arrives
                std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay_ms));
                continue;
            }

            frame_copy = latest_frame_for_stream_;  // shallow cv::Mat copy
        }

        {
            std::lock_guard<std::mutex> lock(tracks_mtx_);
            tracks_copy = latest_tracks_;
        }

        {
            ScopedTimer stream_timer("stream.draw_write");

            draw::draw_tracks(frame_copy.image, tracks_copy, class_names_);

            if (!video_writer_opened_) {
                const std::string gst_pipeline = make_output_pipeline();

                std::cerr << "[RTSP] GStreamer output pipeline:\n"
                          << gst_pipeline << "\n";

                video_writer_.open(
                    gst_pipeline,
                    cv::CAP_GSTREAMER,
                    0,
                    output_fps,
                    frame_copy.image.size(),
                    true);

                if (!video_writer_.isOpened()) {
                    std::cerr << "[RTSP] Failed to open GStreamer writer\n";
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                std::cerr << "[RTSP] Publishing stream to "
                          << config_.output.rtsp_url << "\n";

                video_writer_opened_ = true;
            }

            video_writer_.write(frame_copy.image);
            g_metrics.increment("stream.frames");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(frame_delay_ms));
    }

    close_outputs();
}

void Pipeline::metrics_loop() {
    while (running.load()) {
        std::this_thread::sleep_for(
            std::chrono::seconds(config_.metrics.report_interval_s));

        g_metrics.record("cap.fps", cap_fps_.get_fps());
        g_metrics.report_and_reset();
    }
}

void Pipeline::close_outputs() {
    if (video_writer_opened_) {
        std::cerr << "[RTSP] Closing video writer...\n";
        video_writer_.release();
        video_writer_opened_ = false;
    }
}

void Pipeline::run() {
    g_pipeline = this;  // register this instance globally

    // Register signal handlers
    std::signal(SIGINT, signal_handler);   // Ctrl+C
    std::signal(SIGTERM, signal_handler);  // systemd stop

    std::thread t1(&Pipeline::capture_loop, this);
    std::thread t2(&Pipeline::motion_loop, this);
    std::thread t3(&Pipeline::detect_loop, this);
    std::thread t4(&Pipeline::stream_loop, this);
    std::thread t5(&Pipeline::metrics_loop, this);

    // Main thread waits until running becomes false
    while (running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    detector_condition_.notify_all();

    t1.join();
    t2.join();
    t3.join();
    t4.join();
    t5.join();

    close_outputs();
}