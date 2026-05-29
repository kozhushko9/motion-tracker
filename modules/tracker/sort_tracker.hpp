#pragma once

#include <chrono>
#include <opencv2/core.hpp>
#include <opencv2/video/tracking.hpp>
#include <optional>
#include <vector>

struct Track {
    int id;
    cv::Rect box;

    int age = 0;
    int hits = 0;
    int missed = 0;

    int class_id = -1;
    float score = 0.0f;
};

class SortTracker {
   public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    SortTracker(int max_missed = 10,
                float iou_thresh = 0.2f,
                int min_hits = 1);

    std::vector<Track> update(const std::vector<cv::Rect>& detections,
                              const std::vector<int>& class_ids,
                              const std::vector<float>& scores,
                              std::optional<time_point> timestamp = std::nullopt);

   private:
    struct KalmanTrack {
        int id = -1;

        cv::KalmanFilter kf;
        cv::Rect box;

        int age = 0;
        int hits = 0;
        int missed = 0;
        int hit_streak = 0;

        int class_id = -1;
        float score = 0.0f;
    };

    std::vector<KalmanTrack> tracks_;

    int next_id_ = 0;
    int frame_count_ = 0;

    int max_missed_;
    float iou_thresh_;
    int min_hits_;

    std::optional<time_point> last_timestamp_;

    static cv::Mat rect_to_measurement(const cv::Rect& box);
    static cv::Rect state_to_rect(const cv::Mat& state);

    static KalmanTrack create_track(int id,
                                    const cv::Rect& box,
                                    int class_id,
                                    float score);

    static cv::Rect predict_track(KalmanTrack& track, float dt);

    static void update_track(KalmanTrack& track,
                             const cv::Rect& detection,
                             int class_id,
                             float score);

    static std::vector<int> hungarian_solve(const std::vector<std::vector<float>>& cost_matrix);
};