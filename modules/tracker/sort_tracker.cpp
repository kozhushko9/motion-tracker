#include "sort_tracker.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "vision_utils.hpp"

SortTracker::SortTracker(int max_missed,
                         float iou_thresh,
                         int min_hits)
    : max_missed_(max_missed),
      iou_thresh_(iou_thresh),
      min_hits_(min_hits) {
}

cv::Mat SortTracker::rect_to_measurement(const cv::Rect& box) {
    const float width = static_cast<float>(std::max(1, box.width));
    const float height = static_cast<float>(std::max(1, box.height));

    const float cx = static_cast<float>(box.x) + width * 0.5f;
    const float cy = static_cast<float>(box.y) + height * 0.5f;
    const float area = width * height;
    const float ratio = width / height;

    cv::Mat measurement(4, 1, CV_32F);
    measurement.at<float>(0) = cx;
    measurement.at<float>(1) = cy;
    measurement.at<float>(2) = area;
    measurement.at<float>(3) = ratio;

    return measurement;
}

cv::Rect SortTracker::state_to_rect(const cv::Mat& state) {
    const float cx = state.at<float>(0);
    const float cy = state.at<float>(1);

    float area = state.at<float>(2);
    float ratio = state.at<float>(3);

    area = std::max(area, 1.0f);
    ratio = std::max(ratio, 1e-3f);

    const float width = std::sqrt(area * ratio);
    const float height = area / std::max(width, 1e-3f);

    const int x = static_cast<int>(std::round(cx - width * 0.5f));
    const int y = static_cast<int>(std::round(cy - height * 0.5f));
    const int w = static_cast<int>(std::round(width));
    const int h = static_cast<int>(std::round(height));

    return {
        x,
        y,
        std::max(1, w),
        std::max(1, h)};
}

SortTracker::KalmanTrack SortTracker::create_track(int id,
                                                   const cv::Rect& box,
                                                   int class_id,
                                                   float score) {
    KalmanTrack track;

    track.id = id;
    track.box = box;

    track.age = 1;
    track.hits = 1;
    track.missed = 0;
    track.hit_streak = 1;

    track.class_id = class_id;
    track.score = score;

    // State:
    // [cx, cy, s, r, vx, vy, vs]
    //
    // cx, cy = box center
    // s      = scale / area
    // r      = aspect ratio width / height
    // vx     = velocity of cx
    // vy     = velocity of cy
    // vs     = velocity of scale
    //
    // Measurement:
    // [cx, cy, s, r]
    track.kf.init(7, 4, 0, CV_32F);

    // Initial transition matrix with dt = 1
    track.kf.transitionMatrix = (cv::Mat_<float>(7, 7) << 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1);

    track.kf.measurementMatrix = cv::Mat::zeros(4, 7, CV_32F);
    track.kf.measurementMatrix.at<float>(0, 0) = 1.0f;
    track.kf.measurementMatrix.at<float>(1, 1) = 1.0f;
    track.kf.measurementMatrix.at<float>(2, 2) = 1.0f;
    track.kf.measurementMatrix.at<float>(3, 3) = 1.0f;

    // Smoother tracking: Lower process noise (1e-2 -> 1e-3)
    // and higher measurement noise (1e-1 -> 1.0)
    // to trust the model less and the history more.
    setIdentity(track.kf.processNoiseCov, cv::Scalar::all(1e-3));
    setIdentity(track.kf.measurementNoiseCov, cv::Scalar::all(1.0));
    setIdentity(track.kf.errorCovPost, cv::Scalar::all(1.0));

    cv::Mat measurement = rect_to_measurement(box);

    track.kf.statePost = cv::Mat::zeros(7, 1, CV_32F);
    track.kf.statePost.at<float>(0) = measurement.at<float>(0);
    track.kf.statePost.at<float>(1) = measurement.at<float>(1);
    track.kf.statePost.at<float>(2) = measurement.at<float>(2);
    track.kf.statePost.at<float>(3) = measurement.at<float>(3);

    track.kf.statePost.at<float>(4) = 0.0f;
    track.kf.statePost.at<float>(5) = 0.0f;
    track.kf.statePost.at<float>(6) = 0.0f;

    return track;
}

cv::Rect SortTracker::predict_track(KalmanTrack& track, float dt) {
    // Update transition matrix with actual time step
    track.kf.transitionMatrix.at<float>(0, 4) = dt;
    track.kf.transitionMatrix.at<float>(1, 5) = dt;
    track.kf.transitionMatrix.at<float>(2, 6) = dt;

    cv::Mat prediction = track.kf.predict();

    track.age++;
    track.missed++;

    if (track.missed > 0) {
        track.hit_streak = 0;
    }

    track.box = state_to_rect(prediction);

    return track.box;
}

void SortTracker::update_track(KalmanTrack& track,
                               const cv::Rect& detection,
                               int class_id,
                               float score) {
    cv::Mat measurement = rect_to_measurement(detection);

    cv::Mat corrected = track.kf.correct(measurement);

    track.box = state_to_rect(corrected);

    track.class_id = class_id;
    track.score = score;

    track.hits++;
    track.hit_streak++;
    track.missed = 0;
}

std::vector<int> SortTracker::hungarian_solve(const std::vector<std::vector<float>>& cost_matrix) {
    if (cost_matrix.empty()) {
        return {};
    }

    const int rows = static_cast<int>(cost_matrix.size());
    const int cols = static_cast<int>(cost_matrix[0].size());

    if (cols == 0) {
        return std::vector<int>(rows, -1);
    }

    for (const auto& row : cost_matrix) {
        if (static_cast<int>(row.size()) != cols) {
            throw std::runtime_error("Hungarian solver received a non-rectangular cost matrix");
        }
    }

    if (rows > cols) {
        std::vector<std::vector<float>> transposed(cols, std::vector<float>(rows));
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                transposed[c][r] = cost_matrix[r][c];
            }
        }
        std::vector<int> transposed_assignment = hungarian_solve(transposed);
        std::vector<int> assignment(rows, -1);
        for (int c = 0; c < cols; ++c) {
            const int r = transposed_assignment[c];
            if (r >= 0 && r < rows) {
                assignment[r] = c;
            }
        }
        return assignment;
    }

    const int n = rows;
    const int m = cols;

    std::vector<float> u(n + 1, 0.0f);
    std::vector<float> v(m + 1, 0.0f);
    std::vector<int> p(m + 1, 0);
    std::vector<int> way(m + 1, 0);

    for (int i = 1; i <= n; ++i) {
        p[0] = i;
        int j0 = 0;
        std::vector<float> minv(m + 1, std::numeric_limits<float>::max());
        std::vector<char> used(m + 1, false);
        do {
            used[j0] = true;
            const int i0 = p[j0];
            float delta = std::numeric_limits<float>::max();
            int j1 = 0;
            for (int j = 1; j <= m; ++j) {
                if (used[j]) continue;
                const float cur = cost_matrix[i0 - 1][j - 1] - u[i0] - v[j];
                if (cur < minv[j]) {
                    minv[j] = cur;
                    way[j] = j0;
                }
                if (minv[j] < delta) {
                    delta = minv[j];
                    j1 = j;
                }
            }
            for (int j = 0; j <= m; ++j) {
                if (used[j]) {
                    u[p[j]] += delta;
                    v[j] -= delta;
                } else {
                    minv[j] -= delta;
                }
            }
            j0 = j1;
        } while (p[j0] != 0);
        do {
            const int j1 = way[j0];
            p[j0] = p[j1];
            j0 = j1;
        } while (j0 != 0);
    }
    std::vector<int> assignment(rows, -1);
    for (int j = 1; j <= m; ++j) {
        if (p[j] != 0) assignment[p[j] - 1] = j - 1;
    }
    return assignment;
}

std::vector<Track> SortTracker::update(const std::vector<cv::Rect>& detections,
                                       const std::vector<int>& class_ids,
                                       const std::vector<float>& scores,
                                       std::optional<time_point> timestamp) {
    if (detections.size() != class_ids.size() || detections.size() != scores.size()) {
        throw std::runtime_error("SortTracker::update received mismatched detection vector sizes");
    }

    frame_count_++;

    float dt = 1.0f;  // Default for non-time-aware mode
    if (timestamp.has_value()) {
        if (last_timestamp_.has_value()) {
            dt = std::chrono::duration<float>(timestamp.value() - last_timestamp_.value()).count();
        }
        last_timestamp_ = timestamp;
    }

    // ------------------------------------------------------------
    // 1. Predict all existing tracks using Kalman filter.
    // ------------------------------------------------------------
    std::vector<cv::Rect> predicted_boxes;
    predicted_boxes.reserve(tracks_.size());

    for (auto& track : tracks_) {
        predicted_boxes.push_back(predict_track(track, dt));
    }

    // ------------------------------------------------------------
    // 2. Match tracks to detections using IoU + Centroid fallback.
    // ------------------------------------------------------------
    std::vector<int> track_to_detection(tracks_.size(), -1);
    std::vector<int> detection_used(detections.size(), 0);

    if (!tracks_.empty() && !detections.empty()) {
        std::vector<std::vector<float>> cost_matrix(
            tracks_.size(),
            std::vector<float>(detections.size(), 0.0f));

        for (size_t t = 0; t < tracks_.size(); ++t) {
            for (size_t d = 0; d < detections.size(); ++d) {
                if (tracks_[t].class_id != -1 && tracks_[t].class_id != class_ids[d]) {
                    cost_matrix[t][d] = 1e6f;
                    continue;
                }

                const float iou_score = vision::iou(predicted_boxes[t], detections[d]);

                if (iou_score > 0.0f) {
                    cost_matrix[t][d] = 1.0f - iou_score;
                } else {
                    // Fallback: Normalized Centroid Distance
                    const cv::Point2f p1(
                        predicted_boxes[t].x + predicted_boxes[t].width * 0.5f,
                        predicted_boxes[t].y + predicted_boxes[t].height * 0.5f);
                    const cv::Point2f p2(
                        detections[d].x + detections[d].width * 0.5f,
                        detections[d].y + detections[d].height * 0.5f);
                    const float dist = cv::norm(p1 - p2);
                    const float diag = std::sqrt(
                        static_cast<float>(predicted_boxes[t].width * predicted_boxes[t].width +
                                           predicted_boxes[t].height * predicted_boxes[t].height));
                    cost_matrix[t][d] = 1.0f + (dist / std::max(1.0f, diag));
                }
            }
        }

        std::vector<int> assignment = hungarian_solve(cost_matrix);

        for (size_t t = 0; t < assignment.size(); ++t) {
            const int d = assignment[t];
            if (d < 0) continue;

            const float iou_score = vision::iou(predicted_boxes[t], detections[d]);
            if (iou_score >= iou_thresh_) {
                track_to_detection[t] = d;
                detection_used[d] = 1;
            } else if (iou_score <= 0.0f) {
                if (cost_matrix[t][d] < 2.0f) {
                    track_to_detection[t] = d;
                    detection_used[d] = 1;
                }
            }
        }
    }

    // ------------------------------------------------------------
    // 3. Update matched tracks.
    // ------------------------------------------------------------
    for (size_t t = 0; t < tracks_.size(); ++t) {
        const int d = track_to_detection[t];
        if (d >= 0) {
            update_track(tracks_[t], detections[d], class_ids[d], scores[d]);
        }
    }

    // ------------------------------------------------------------
    // 4. Create new tracks for unmatched detections.
    // ------------------------------------------------------------
    for (int d = 0; d < static_cast<int>(detections.size()); ++d) {
        if (!detection_used[d]) {
            tracks_.push_back(create_track(next_id_++, detections[d], class_ids[d], scores[d]));
        }
    }

    // ------------------------------------------------------------
    // 5. Remove dead tracks.
    // ------------------------------------------------------------
    tracks_.erase(
        std::remove_if(
            tracks_.begin(),
            tracks_.end(),
            [&](const KalmanTrack& track) {
                return track.missed > max_missed_;
            }),
        tracks_.end());

    // ------------------------------------------------------------
    // 6. Return public Track objects.
    // ------------------------------------------------------------
    std::vector<Track> output;
    output.reserve(tracks_.size());

    for (const auto& track : tracks_) {
        const bool confirmed = track.hits >= min_hits_;
        const bool startup_phase = frame_count_ <= min_hits_;
        if (track.missed == 0 && (confirmed || startup_phase)) {
            output.push_back(
                Track{track.id, track.box, track.age, track.hits, track.missed, track.class_id, track.score});
        }
    }

    return output;
}