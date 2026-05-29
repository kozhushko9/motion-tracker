#pragma once
#include "frame.hpp"
#include <opencv2/core.hpp>

class Motion {
public:
    Motion(int t_threshold = 200);
    bool detect(const Frame& frame);

private:
    cv::Mat prev_gray;
    int threshold;
};