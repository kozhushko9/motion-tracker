#include <map>
#include <opencv2/core.hpp>

#include "sort_tracker.hpp"

namespace draw {

void draw_tracks(
    cv::Mat& display,
    const std::vector<Track>& tracks,
    const std::map<int, std::string>& class_names) {
    const double scale_factor = static_cast<double>(display.cols) / 640.0;
    const double font_scale = 0.6 * scale_factor;
    const int thickness = std::max(1, static_cast<int>(1.5 * scale_factor));
    const int font_face = cv::FONT_HERSHEY_DUPLEX;

    for (const Track& t : tracks) {
        if (t.hits < 2) {
            continue;
        }

        const cv::Scalar color(0, 255, 0);

        cv::rectangle(display, t.box, color, thickness);

        auto it = class_names.find(t.class_id);
        const std::string class_name =
            (it != class_names.end()) ? it->second : std::to_string(t.class_id);

        std::string label = std::format("{} id:{} {:.2f}",
                                        class_name,
                                        t.id,
                                        t.score);

        int baseline = 0;
        cv::Size text_size = cv::getTextSize(
            label,
            font_face,
            font_scale,
            thickness,
            &baseline);

        cv::Point text_org(t.box.x, t.box.y - static_cast<int>(5 * scale_factor));

        cv::Rect bg_rect(
            text_org.x,
            text_org.y - text_size.height,
            text_size.width,
            text_size.height + baseline);

        if (bg_rect.y < 0) {
            bg_rect.y = t.box.y;
            text_org.y = t.box.y + text_size.height;
        }

        cv::rectangle(display, bg_rect, color, cv::FILLED);

        cv::putText(display,
                    label,
                    text_org,
                    font_face,
                    font_scale,
                    cv::Scalar(0, 0, 0),
                    thickness,
                    cv::LINE_AA);
    }
}

}  // namespace
