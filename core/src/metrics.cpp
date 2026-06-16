#include "opena8djcpp/metrics.hpp"

namespace opena8djcpp {

bool offline_gate_passes(const MetricsSnapshot& snapshot) {
  return snapshot.input.underruns == 0 && snapshot.input.overruns == 0 &&
         snapshot.output.underruns == 0 && snapshot.output.overruns == 0 &&
         snapshot.input.route_mismatches == 0 && snapshot.output.route_mismatches == 0 &&
         snapshot.input.timestamp_regressions == 0 &&
         snapshot.output.timestamp_regressions == 0;
}

}  // namespace opena8djcpp
