#pragma once
#include <opencv2/core.hpp>
#include <vector>

struct DetectionResult {
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    std::vector<int> class_ids;

    bool empty() const {
        return boxes.empty();
    }

    std::size_t size() const {
        return boxes.size();
    }
};