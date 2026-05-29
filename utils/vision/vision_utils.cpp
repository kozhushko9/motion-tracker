#include "vision_utils.hpp"

#include <algorithm>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace vision {

// Resize with letterbox padding
void letterbox(
    const cv::Mat& frame,
    cv::Mat& output,
    cv::Mat& resized_buffer,
    int target_size,
    int& top,
    int& left,
    float& scale) {
    if (frame.empty()) {
        throw std::invalid_argument("Letterbox: input frame is empty");
    }

    int input_h = frame.rows;
    int input_w = frame.cols;

    scale = std::min(static_cast<float>(target_size) / input_h,
                     static_cast<float>(target_size) / input_w);

    int new_h = static_cast<int>(std::round(input_h * scale));
    int new_w = static_cast<int>(std::round(input_w * scale));

    cv::resize(frame, resized_buffer, cv::Size(new_w, new_h));

    if (output.size() != cv::Size(target_size, target_size) || output.type() != CV_8UC3) {
        output = cv::Mat(target_size, target_size, CV_8UC3, cv::Scalar(114, 114, 114));
    } else {
        output.setTo(cv::Scalar(114, 114, 114));
    }

    top = (target_size - new_h) / 2;
    left = (target_size - new_w) / 2;

    resized_buffer.copyTo(output(cv::Rect(left, top, new_w, new_h)));
}

// Intersection-over-Union
float iou(const cv::Rect& a, const cv::Rect& b) {
    const cv::Rect intersection = a & b;

    const float intersection_area =
        static_cast<float>(intersection.area());

    const float area_a = static_cast<float>(a.area());
    const float area_b = static_cast<float>(b.area());

    const float union_area = area_a + area_b - intersection_area;

    if (union_area <= 0.0f) {
        return 0.0f;
    }

    return intersection_area / union_area;
}
}  // namespace vision
