#pragma once
#include <chrono>
#include <string>

class ScopedTimer {
public:
  explicit ScopedTimer(const char *name);
  ~ScopedTimer();

private:
  const char *name_;
  std::chrono::steady_clock::time_point start_;
};
