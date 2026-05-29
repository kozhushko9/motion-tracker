#pragma once

#include <chrono>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "config.hpp"

struct Metric {
    double sum = 0.0;
    size_t count = 0;
    double min = 0.0;
    double max = 0.0;
    double last = 0.0;
    std::vector<double> samples;

    Metric();

    void add(double ms);
    double avg() const;
    double percentile(double p) const;
    void reset(bool keep_last = false);
};

class MetricsRegistry {
   public:
    using clock = std::chrono::steady_clock;

    void open_csv_log(const MetricsConfig& config);

    void record(const char* name, double ms);
    void increment(const char* name, size_t value = 1);
    void report_and_reset(std::ostream& os = std::cerr);

   private:
    std::mutex mtx_;
    std::map<std::string, Metric> metrics_;
    std::unordered_map<std::string, size_t> counters_;

    MetricsConfig config_;

    std::ofstream csv_file_;
    bool csv_header_written_ = false;

    double get_cpu_temp();
    size_t get_mem_usage();
    void update_system_metrics();

    clock::time_point last_report_time_{clock::now()};
};

extern MetricsRegistry g_metrics;
