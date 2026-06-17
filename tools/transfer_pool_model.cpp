#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Scenario {
  const char* name = "";
  std::uint32_t periods = 1000;
  std::uint32_t capture_depth = 64;
  std::uint32_t capture_pool_slots = 64;
  std::uint32_t playback_queue_target = 64;
  std::uint32_t playback_pool_slots = 128;
  std::uint32_t capture_paced_output_lead = 1;
  std::uint32_t playback_coalesce = 1;
  std::uint32_t playback_completion_periods = 1;
  std::uint32_t leaked_capture_slots = 0;
  std::uint32_t leaked_playback_slots = 0;
  double min_playback_queue_ratio = 0.95;
  double max_playback_queue_ratio = 1.05;
  bool expect_no_fallback = true;
  bool expect_transport_rate_safe = true;
};

struct Result {
  std::uint64_t capture_queue_attempts = 0;
  std::uint64_t playback_queue_attempts = 0;
  std::uint64_t capture_fallback_allocations = 0;
  std::uint64_t playback_fallback_allocations = 0;
  std::uint32_t capture_in_flight_max = 0;
  std::uint32_t playback_in_flight_max = 0;
  double playback_queue_ratio = 0.0;
  bool transport_rate_safe = false;
};

struct PlaybackTransfer {
  std::uint32_t remaining_periods = 0;
  bool pooled = true;
};

bool checkout_from_pool(std::uint32_t pool_slots,
                        std::uint32_t leaked_slots,
                        std::uint32_t in_flight) {
  return leaked_slots + in_flight < pool_slots;
}

void complete_playback(std::vector<PlaybackTransfer>& transfers) {
  for (auto& transfer : transfers) {
    if (transfer.remaining_periods > 0) {
      transfer.remaining_periods -= 1;
    }
  }
  transfers.erase(std::remove_if(transfers.begin(),
                                 transfers.end(),
                                 [](const PlaybackTransfer& transfer) {
                                   return transfer.remaining_periods == 0;
                                 }),
                  transfers.end());
}

void queue_playback(const Scenario& scenario,
                    Result& result,
                    std::vector<PlaybackTransfer>& playback) {
  result.playback_queue_attempts += 1;
  const std::uint32_t in_flight = static_cast<std::uint32_t>(playback.size());
  const bool pooled = checkout_from_pool(scenario.playback_pool_slots,
                                         scenario.leaked_playback_slots,
                                         in_flight);
  if (!pooled) {
    result.playback_fallback_allocations += 1;
  }
  playback.push_back(PlaybackTransfer{scenario.playback_completion_periods, pooled});
  result.playback_in_flight_max =
      std::max(result.playback_in_flight_max, static_cast<std::uint32_t>(playback.size()));
}

Result run(const Scenario& scenario) {
  Result result{};
  std::uint32_t capture_in_flight = 0;
  for (std::uint32_t index = 0; index < scenario.capture_depth; ++index) {
    result.capture_queue_attempts += 1;
    if (!checkout_from_pool(scenario.capture_pool_slots,
                            scenario.leaked_capture_slots,
                            capture_in_flight)) {
      result.capture_fallback_allocations += 1;
    }
    capture_in_flight += 1;
  }
  result.capture_in_flight_max = capture_in_flight;

  std::vector<PlaybackTransfer> playback;
  playback.reserve(scenario.playback_pool_slots + 16U);
  std::uint32_t pending_playback_requests = 0;
  const std::uint32_t playback_queue_max = scenario.playback_queue_target * 2U;
  const std::uint32_t desired_lead =
      std::min(std::max(1U, scenario.capture_paced_output_lead), playback_queue_max);
  const std::uint32_t coalesce = std::max(1U, scenario.playback_coalesce);

  for (std::uint32_t period = 0; period < scenario.periods; ++period) {
    complete_playback(playback);

    if (capture_in_flight > 0) {
      capture_in_flight -= 1;
    }
    result.capture_queue_attempts += 1;
    if (!checkout_from_pool(scenario.capture_pool_slots,
                            scenario.leaked_capture_slots,
                            capture_in_flight)) {
      result.capture_fallback_allocations += 1;
    }
    capture_in_flight += 1;
    result.capture_in_flight_max = std::max(result.capture_in_flight_max, capture_in_flight);

    pending_playback_requests += 1;
    if (pending_playback_requests < coalesce) {
      continue;
    }
    pending_playback_requests = 0;

    while (playback.size() < desired_lead && playback.size() < playback_queue_max) {
      queue_playback(scenario, result, playback);
    }
  }

  result.playback_queue_ratio = scenario.periods == 0
                                    ? 0.0
                                    : static_cast<double>(result.playback_queue_attempts) /
                                          static_cast<double>(scenario.periods);
  result.transport_rate_safe =
      result.playback_queue_ratio >= scenario.min_playback_queue_ratio &&
      result.playback_queue_ratio <= scenario.max_playback_queue_ratio;
  return result;
}

bool passes(const Scenario& scenario, const Result& result) {
  const bool no_fallback =
      result.capture_fallback_allocations == 0 && result.playback_fallback_allocations == 0;
  return no_fallback == scenario.expect_no_fallback &&
         result.transport_rate_safe == scenario.expect_transport_rate_safe;
}

void print_row(const Scenario& scenario, const Result& result, bool ok, bool trailing_comma) {
  std::cout << "    {\"scenario\": \"" << scenario.name << "\""
            << ", \"periods\": " << scenario.periods
            << ", \"capture_depth\": " << scenario.capture_depth
            << ", \"capture_pool_slots\": " << scenario.capture_pool_slots
            << ", \"playback_queue_target\": " << scenario.playback_queue_target
            << ", \"playback_pool_slots\": " << scenario.playback_pool_slots
            << ", \"capture_paced_output_lead\": " << scenario.capture_paced_output_lead
            << ", \"playback_coalesce\": " << scenario.playback_coalesce
            << ", \"playback_completion_periods\": " << scenario.playback_completion_periods
            << ", \"leaked_capture_slots\": " << scenario.leaked_capture_slots
            << ", \"leaked_playback_slots\": " << scenario.leaked_playback_slots
            << ", \"min_playback_queue_ratio\": " << scenario.min_playback_queue_ratio
            << ", \"max_playback_queue_ratio\": " << scenario.max_playback_queue_ratio
            << ", \"capture_queue_attempts\": " << result.capture_queue_attempts
            << ", \"playback_queue_attempts\": " << result.playback_queue_attempts
            << ", \"playback_queue_ratio\": " << result.playback_queue_ratio
            << ", \"transport_rate_safe\": " << (result.transport_rate_safe ? "true" : "false")
            << ", \"capture_fallback_allocations\": " << result.capture_fallback_allocations
            << ", \"playback_fallback_allocations\": " << result.playback_fallback_allocations
            << ", \"capture_in_flight_max\": " << result.capture_in_flight_max
            << ", \"playback_in_flight_max\": " << result.playback_in_flight_max
            << ", \"expect_no_fallback\": " << (scenario.expect_no_fallback ? "true" : "false")
            << ", \"expect_transport_rate_safe\": "
            << (scenario.expect_transport_rate_safe ? "true" : "false")
            << ", \"result\": \"" << (ok ? "PASS" : "FAIL") << "\"}";
  if (trailing_comma) {
    std::cout << ",";
  }
  std::cout << "\n";
}

}  // namespace

int main() {
  const std::vector<Scenario> scenarios = {
      {
          .name = "default_capture64_playback64_lead1",
      },
      {
          .name = "mainline_like_queue8",
          .capture_depth = 8,
          .capture_pool_slots = 8,
          .playback_queue_target = 8,
          .playback_pool_slots = 16,
      },
      {
          .name = "coalesce2_no_pool_pressure_but_cadence_rejected_elsewhere",
          .playback_coalesce = 2,
          .playback_completion_periods = 2,
          .expect_transport_rate_safe = false,
      },
      {
          .name = "lead2_implicit_bursts_rejected",
          .capture_paced_output_lead = 2,
          .expect_transport_rate_safe = false,
      },
      {
          .name = "lead4_implicit_bursts_rejected",
          .capture_paced_output_lead = 4,
          .expect_transport_rate_safe = false,
      },
      {
          .name = "lead64_pool_margin",
          .capture_paced_output_lead = 64,
          .expect_transport_rate_safe = false,
      },
      {
          .name = "capture_pool_leak_rejected",
          .leaked_capture_slots = 1,
          .expect_no_fallback = false,
      },
      {
          .name = "playback_pool_leak_rejected",
          .leaked_playback_slots = 128,
          .expect_no_fallback = false,
      },
  };

  std::uint32_t failures = 0;
  std::cout << "{\n  \"schema\": \"opena8djcpp.transfer-pool-model.v1\",\n  \"rows\": [\n";
  for (std::size_t index = 0; index < scenarios.size(); ++index) {
    const Result result = run(scenarios[index]);
    const bool ok = passes(scenarios[index], result);
    if (!ok) {
      failures += 1;
    }
    print_row(scenarios[index], result, ok, index + 1U < scenarios.size());
  }
  std::cout << "  ],\n"
            << "  \"row_count\": " << scenarios.size() << ",\n"
            << "  \"failures\": " << failures << ",\n"
            << "  \"result\": \"" << (failures == 0 ? "PASS" : "FAIL") << "\"\n"
            << "}\n";

  return failures == 0 ? 0 : 1;
}
