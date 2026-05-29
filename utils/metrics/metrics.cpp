#include "metrics.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <vector>

namespace {
// clang-format off
bool has_metric(const std::map<std::string, Metric>& metrics,
                const std::string& key) {
    auto it = metrics.find(key);
    return it != metrics.end() && it->second.count > 0;
}

bool section_has_any_metric(
    const std::map<std::string, Metric>& metrics,
    const std::vector<std::pair<const char*, const char*>>& items) {
    for (const auto& [label, key] : items) {
        (void)label;

        if (has_metric(metrics, key)) {
            return true;
        }
    }
    return false;
}

int precision_for_ms(double value) {
    if (value < 1.0) {
        return 3;
    }

    if (value < 10.0) {
        return 2;
    }

    return 1;
}

void print_ms_value(std::ostream& os, double value) {
    os << std::fixed << std::setprecision(precision_for_ms(value))
       << std::setw(9) << value;
}

size_t get_counter(
    const std::unordered_map<std::string, size_t>& counters,
    const std::string& key
) {
    auto it = counters.find(key);
    return it == counters.end() ? 0 : it->second;
}

std::string make_timestamp(const char* format) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};

    // Local time on the machine: Ubuntu/Pi timezone
    localtime_r(&t, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, format);

    return oss.str();
}

}  // namespace

Metric::Metric()
    : sum(0.0),
      count(0),
      min(std::numeric_limits<double>::max()),
      max(std::numeric_limits<double>::lowest()),
      last(0.0) {
}

void Metric::add(double ms) {
    sum += ms;
    count++;
    min = std::min(min, ms);
    max = std::max(max, ms);
    last = ms;

    // Keep memory bounded during one report window
    if (samples.size() < 1000) {
        samples.push_back(ms);
    }
}

double Metric::avg() const {
    return count > 0 ? sum / static_cast<double>(count) : 0.0;
}

double Metric::percentile(double p) const {
    if (samples.empty()) return 0.0;

    auto s = samples;
    std::sort(s.begin(), s.end());

    if (p <= 0.0) return s.front();

    if (p >= 1.0) return s.back();

    // Nearest-rank percentile.
    size_t idx = static_cast<size_t>(std::ceil(p * s.size())) - 1;
    idx = std::min(idx, s.size() - 1);

    return s[idx];
}

void Metric::reset(bool keep_last) {
    sum = 0.0;
    count = 0;
    min = std::numeric_limits<double>::max();
    max = std::numeric_limits<double>::lowest();
    samples.clear();

    if (!keep_last) {
        last = 0.0;
    }
}

MetricsRegistry g_metrics;

void MetricsRegistry::open_csv_log(const MetricsConfig& config) {
    std::lock_guard<std::mutex> lock(mtx_);

    config_ = config;

    if (config_.csv_path.empty()) {
        return;
    }

    std::filesystem::path csv_path(config_.csv_path);

    if (csv_path.has_parent_path()) {
        std::filesystem::create_directories(csv_path.parent_path());
    }

    csv_file_.open(config_.csv_path, std::ios::out | std::ios::app);

    if (!csv_file_.is_open()) {
        std::cerr << "[Metrics] Failed to open CSV file: "
                  << config_.csv_path << "\n";
        return;
    }

    csv_file_.seekp(0, std::ios::end);

    if (csv_file_.tellp() == 0) {
        csv_file_
            << "timestamp_iso,"
            << "cpu_temp_c,"
            << "mem_usage_mb,"
            << "cap_fps,"
            << "det_runs_per_sec,"
            << "pos_frames_per_min,"
            << "obj_per_min,"
            << "cap_read_avg_ms,"
            << "motion_det_avg_ms,"
            << "det_preprocess_avg_ms,"
            << "det_inference_avg_ms,"
            << "det_decode_avg_ms,"
            << "det_total_avg_ms,"
            << "tracker_update_avg_ms,"
            << "show_save_avg_ms,"
            << "pipeline_latency_avg_ms,"
            << "pipeline_latency_p90_ms,"
            << "pipeline_latency_p95_ms"
            << "\n";

        csv_file_.flush();
    }

    std::cout << "[Metrics] CSV logging enabled: "
              << config_.csv_path << "\n";
}

double MetricsRegistry::get_cpu_temp() {
    std::ifstream fs("/sys/class/thermal/thermal_zone0/temp");

    if (!fs.is_open()) return 0.0;

    double temp_raw = 0.0;
    fs >> temp_raw;

    return temp_raw / 1000.0;
}

size_t MetricsRegistry::get_mem_usage() {
    std::ifstream fs("/proc/self/status");

    if (!fs.is_open()) return 0;

    std::string line;

    while (std::getline(fs, line)) {
        if (line.compare(0, 6, "VmRSS:") == 0) {
            size_t kb = static_cast<size_t>(std::stoull(line.substr(7)));
            return kb / 1024;  // MB
        }
    }
    return 0;
}

void MetricsRegistry::update_system_metrics() {
    const double temp = get_cpu_temp();
    const size_t mem = get_mem_usage();

    std::lock_guard<std::mutex> lock(mtx_);

    metrics_["sys.cpu_temp"].add(temp);
    counters_["sys.mem_usage_mb"] = mem;
}

void MetricsRegistry::record(const char* name, double ms) {
    std::lock_guard<std::mutex> lock(mtx_);
    metrics_[name].add(ms);
}

void MetricsRegistry::increment(const char* name, size_t value) {
    std::lock_guard<std::mutex> lock(mtx_);
    counters_[name] += value;
}

void MetricsRegistry::report_and_reset(std::ostream& os) {
    update_system_metrics();

    std::lock_guard<std::mutex> lock(mtx_);

    const auto now = clock::now();
    const double report_seconds =
        std::chrono::duration<double>(now - last_report_time_).count();

    last_report_time_ = now;

    double temp =
        metrics_.count("sys.cpu_temp") ? metrics_["sys.cpu_temp"].last : 0.0;

    size_t mem = get_counter(counters_, "sys.mem_usage_mb");

    double cap_fps =
        metrics_.count("cap.fps") ? metrics_["cap.fps"].last : 0.0;

    size_t detector_runs = get_counter(counters_, "detector.runs");
    size_t positive_frames = get_counter(counters_, "detector.positive_frames");
    size_t detected_objects = get_counter(counters_, "detector.objects");

    double detector_runs_per_sec =
        report_seconds > 0.0
            ? static_cast<double>(detector_runs) / report_seconds
            : 0.0;

    double positive_frames_per_min =
        report_seconds > 0.0
            ? static_cast<double>(positive_frames) * 60.0 / report_seconds
            : 0.0;

    double objects_per_min =
        report_seconds > 0.0
            ? static_cast<double>(detected_objects) * 60.0 / report_seconds
            : 0.0;

    if (csv_file_.is_open()) {

        const std::string timestamp_iso = make_timestamp("%Y-%m-%d %H:%M:%S");

        csv_file_ << timestamp_iso << ","
                  << temp << "," 
                  << mem << "," 
                  << cap_fps << ","
                  << detector_runs_per_sec << "," 
                  << positive_frames_per_min << ","
                  << objects_per_min << "," 
                  << metrics_["cap.read"].avg() << ","
                  << metrics_["motion.detect"].avg() << ","
                  << metrics_["detector.preprocess"].avg() << ","
                  << metrics_["detector.invoke"].avg() << ","
                  << metrics_["detector.decode"].avg() << ","
                  << metrics_["detector.total"].avg() << ","
                  << metrics_["tracker.update"].avg() << ","
                  << metrics_["output.show_save"].avg() << ","
                  << metrics_["pipeline.latency"].avg() << ","
                  << metrics_["pipeline.latency"].percentile(0.90) << ","
                  << metrics_["pipeline.latency"].percentile(0.95) << "\n";
        csv_file_.flush();
    }

    if (!config_.enable_printing) {
        // Still need to reset metrics even if not printing
        for (auto& [name, m] : metrics_) {
            bool keep_last = (name == "cap.fps" || name == "det.fps" || name == "sys.cpu_temp");
            m.reset(keep_last);
        }
        counters_.clear();
        return;
    }

    auto print_metric = [&](const char* label, const std::string& key) -> bool {
        auto it = metrics_.find(key);

        if (it == metrics_.end() || it->second.count == 0) {
            return false;
        }

        auto& m = it->second;

        os << std::left << std::setw(25) << label << std::right;

        print_ms_value(os, m.avg());
        print_ms_value(os, m.percentile(0.50));
        print_ms_value(os, m.percentile(0.90));
        print_ms_value(os, m.percentile(0.95));

        os << "   n=" << m.count << "\n";

        m.reset(false);
        return true;
    };

    auto print_section = [&](const char* section_name,
        const std::vector<std::pair<const char*, const char*>>& items) {
            
        if (!section_has_any_metric(metrics_, items)) {
            return;
        }

        os << std::string(78, '-') << "\n";
        os << section_name << "\n";
        os << std::string(78, '-') << "\n";

        for (const auto& [label, key] : items) {
            print_metric(label, key);
        }
    };

    os << "\n" << std::string(78, '=') << "\n";
    os << std::fixed << std::setprecision(2) << "CAP FPS: " << cap_fps
    //    << " | DET FPS: " << det_fps
       << " | DET RUNS/s: " << detector_runs_per_sec
       << " | POS DET/min: " << positive_frames_per_min
       << " | OBJ/min: " << objects_per_min << std::setprecision(1)
       << " | Temp: " << temp << "C"
       << " | Mem: " << mem << "MB"
       << "\n";

    os << std::string(78, '-') << "\n";

    os << std::left 
       << std::setw(25) << "LATENCY (ms)" 
       << std::right 
       << std::setw(9) << "AVG" 
       << std::setw(9) << "P50" 
       << std::setw(9) << "P90" 
       << std::setw(9) << "P95"
       << "   COUNT\n";

    print_section("Capture / Motion",
                  {{"Capture Read", "cap.read"}, {"Motion Detect", "motion.detect"}});

    print_section("Detector",
                  {{"Detector Preprocess", "detector.preprocess"},
                   {"Detector Inference", "detector.invoke"},
                   {"Detector Decode", "detector.decode"},
                   {"Detector Total", "detector.total"}});

    print_section("Tracker / Output",
                  {{"Tracker Update", "tracker.update"}, {"Show & Save", "output.show_save"}});

    print_section("Pipeline", {{"End-to-End Latency", "pipeline.latency"}});

    if (!counters_.empty()) {
        os << std::string(78, '-') << "\n";
        os << "COUNTERS\n";
        os << std::string(78, '-') << "\n";

        for (const auto& [name, value] : counters_) {
            if (name == "sys.mem_usage_mb") {
                continue;
            }

            os << std::left << std::setw(35) << name << std::right << value << "\n";
        }
    }

    // Keep gauge-like last values, but reset their samples/counts.
    if (metrics_.count("cap.fps")) {
        metrics_["cap.fps"].reset(true);
    }

    if (metrics_.count("sys.cpu_temp")) {
        metrics_["sys.cpu_temp"].reset(true);
    }

    counters_.clear();

    os << std::string(78, '=') << "\n\n";
    // clang-format on
}