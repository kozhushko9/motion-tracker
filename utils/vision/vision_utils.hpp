#pragma once
#include <opencv2/core.hpp>

namespace vision {

// Reuses existing buffers
void letterbox(
    const cv::Mat& frame,
    cv::Mat& output,
    cv::Mat& resized_buffer,
    int target_size,
    int& top,
    int& left,
    float& scale);

// Intersection-over-Union
float iou(
    const cv::Rect& a,
    const cv::Rect& b);
}  // namespace vision