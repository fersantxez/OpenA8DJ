#pragma once

#include <cstdint>

namespace opena8djcpp {

struct StreamCounters {
  std::uint64_t frames_processed = 0;
  std::uint64_t underruns = 0;
  std::uint64_t overruns = 0;
  std::uint64_t route_mismatches = 0;
  std::uint64_t timestamp_regressions = 0;
};

struct JitterCounters {
  std::uint64_t samples = 0;
  std::uint64_t max_abs_error_ns = 0;
  std::uint64_t sum_abs_error_ns = 0;
};

struct MetricsSnapshot {
  StreamCounters input;
  StreamCounters output;
  JitterCounters jitter;
};

[[nodiscard]] bool offline_gate_passes(const MetricsSnapshot& snapshot);

}  // namespace opena8djcpp
