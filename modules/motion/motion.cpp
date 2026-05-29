#include "motion.hpp"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

Motion::Motion(int t_threshold) : threshold(t_threshold) {}

bool Motion::detect(const Frame& frame) {
    cv::Mat gray;

    cv::cvtColor(frame.image, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, {15,15}, 0);

    if (!prev_gray.empty()) {
        cv::Mat diff;
        cv::absdiff(prev_gray, gray, diff);
        cv::threshold(diff, diff, 25, 255, cv::THRESH_BINARY);

        int motion_level = cv::countNonZero(diff);
        prev_gray = gray;
        return motion_level > threshold;
    }

    prev_gray = gray;
    return false;
}