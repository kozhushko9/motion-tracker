#include "scoped_timer.hpp"
#include "metrics.hpp"

ScopedTimer::ScopedTimer(const char *name)
    : name_(name), start_(std::chrono::steady_clock::now()) {}

ScopedTimer::~ScopedTimer() {
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start_).count();
  g_metrics.record(name_, ms);
}
