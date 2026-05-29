#pragma once
#include <map>
#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "sort_tracker.hpp"

namespace draw {

void draw_tracks(cv::Mat& image,
                 const std::vector<Track>& tracks,
                 const std::map<int, std::string>& class_names);

}