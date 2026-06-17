#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr double kQualityGate = 0.98;
constexpr double kDriverCpuP95Gate = 6.5;
constexpr double kLagJumpGate = 0.0;

struct FamilyEvidence {
  const char* name = "";
  std::uint32_t iso_frames = 0;
  std::uint32_t playback_queue_target = 0;
  double best_quality_alignment_score = 0.0;
  double median_driver_cpu_p95 = 0.0;
  double min_driver_cpu_p95 = 0.0;
  double min_lag_jumps_gt_2_frames = 0.0;
  const char* best_run = "";
};

struct FamilyBudget {
  double capture_requeues_per_second = 0.0;
  double playback_queues_per_second = 0.0;
  double usb_enqueue_calls_per_second = 0.0;
  double cpu_gap_to_gate = 0.0;
  double quality_gap_to_gate = 0.0;
  bool quality_pass = false;
  bool driver_cpu_pass = false;
  bool lag_pass = false;
  bool product_candidate = false;
  const char* verdict = "";
};

FamilyBudget budget_for(const FamilyEvidence& family) {
  FamilyBudget out{};
  const double iso = static_cast<double>(std::max(1U, family.iso_frames));
  out.capture_requeues_per_second = 1000.0 / iso;
  out.playback_queues_per_second = 1000.0 / iso;
  out.usb_enqueue_calls_per_second =
      out.capture_requeues_per_second + out.playback_queues_per_second;
  out.cpu_gap_to_gate = family.median_driver_cpu_p95 - kDriverCpuP95Gate;
  out.quality_gap_to_gate = kQualityGate - family.best_quality_alignment_score;
  out.quality_pass = family.best_quality_alignment_score >= kQualityGate;
  out.driver_cpu_pass = family.median_driver_cpu_p95 <= kDriverCpuP95Gate;
  out.lag_pass = family.min_lag_jumps_gt_2_frames <= kLagJumpGate;
  out.product_candidate = out.quality_pass && out.driver_cpu_pass && out.lag_pass;
  if (out.product_candidate) {
    out.verdict = "candidate";
  } else if (!out.quality_pass && !out.driver_cpu_pass) {
    out.verdict = "quality_and_cpu_fail";
  } else if (!out.quality_pass) {
    out.verdict = "quality_fail";
  } else if (!out.driver_cpu_pass) {
    out.verdict = "cpu_fail";
  } else {
    out.verdict = "lag_fail";
  }
  return out;
}

void print_family(const FamilyEvidence& family, const FamilyBudget& budget, bool trailing_comma) {
  std::cout << "    {\"family\": \"" << family.name << "\""
            << ", \"iso_frames\": " << family.iso_frames
            << ", \"playback_queue_target\": " << family.playback_queue_target
            << ", \"estimated_capture_requeues_per_second\": " << budget.capture_requeues_per_second
            << ", \"estimated_playback_queues_per_second\": " << budget.playback_queues_per_second
            << ", \"estimated_usb_enqueue_calls_per_second\": "
            << budget.usb_enqueue_calls_per_second
            << ", \"best_quality_alignment_score\": " << family.best_quality_alignment_score
            << ", \"median_driver_cpu_p95\": " << family.median_driver_cpu_p95
            << ", \"min_driver_cpu_p95\": " << family.min_driver_cpu_p95
            << ", \"min_lag_jumps_gt_2_frames\": " << family.min_lag_jumps_gt_2_frames
            << ", \"quality_gap_to_gate\": " << budget.quality_gap_to_gate
            << ", \"cpu_gap_to_gate\": " << budget.cpu_gap_to_gate
            << ", \"quality_pass\": " << (budget.quality_pass ? "true" : "false")
            << ", \"driver_cpu_pass\": " << (budget.driver_cpu_pass ? "true" : "false")
            << ", \"lag_pass\": " << (budget.lag_pass ? "true" : "false")
            << ", \"product_candidate\": " << (budget.product_candidate ? "true" : "false")
            << ", \"verdict\": \"" << budget.verdict << "\""
            << ", \"best_run\": \"" << family.best_run << "\"}";
  if (trailing_comma) {
    std::cout << ",";
  }
  std::cout << "\n";
}

}  // namespace

int main() {
  const std::vector<FamilyEvidence> families = {
      {
          .name = "iso5_q64",
          .iso_frames = 5,
          .playback_queue_target = 64,
          .best_quality_alignment_score = 0.978049577556115,
          .median_driver_cpu_p95 = 36.9,
          .min_driver_cpu_p95 = 28.5,
          .min_lag_jumps_gt_2_frames = 22.0,
          .best_run = "local-analysis/soundcheck/20260616-capture-detail-irig-pairA-8s-cpp-hal",
      },
      {
          .name = "iso8_q8",
          .iso_frames = 8,
          .playback_queue_target = 8,
          .best_quality_alignment_score = 0.9647236007120401,
          .median_driver_cpu_p95 = 22.4,
          .min_driver_cpu_p95 = 1.4,
          .min_lag_jumps_gt_2_frames = 23.0,
          .best_run = "local-analysis/soundcheck/20260617-cpp-iso8q8-dense-ch12-irig-pairA-12s",
      },
      {
          .name = "iso10_q8",
          .iso_frames = 10,
          .playback_queue_target = 8,
          .best_quality_alignment_score = 0.9693792029177718,
          .median_driver_cpu_p95 = 19.6,
          .min_driver_cpu_p95 = 19.6,
          .min_lag_jumps_gt_2_frames = 35.0,
          .best_run = "local-analysis/soundcheck/20260617-cpp-iso10q8-dense-ch12-irig-pairA-12s",
      },
      {
          .name = "iso12_q8",
          .iso_frames = 12,
          .playback_queue_target = 8,
          .best_quality_alignment_score = 0.9633948716317187,
          .median_driver_cpu_p95 = 16.6,
          .min_driver_cpu_p95 = 16.6,
          .min_lag_jumps_gt_2_frames = 32.0,
          .best_run = "local-analysis/soundcheck/20260617-iso12q8-irig-pairA-12s-cpp-hal",
      },
      {
          .name = "iso64_q8",
          .iso_frames = 64,
          .playback_queue_target = 8,
          .best_quality_alignment_score = 0.6867121056638885,
          .median_driver_cpu_p95 = 6.3,
          .min_driver_cpu_p95 = 6.0,
          .min_lag_jumps_gt_2_frames = 35.0,
          .best_run = "local-analysis/soundcheck/20260617-cpp-iso64q8-stopisoc-irig-pairA-12s",
      },
  };

  std::uint32_t product_candidates = 0;
  std::uint32_t quality_passes = 0;
  std::uint32_t cpu_passes = 0;
  double best_quality = 0.0;
  double lowest_median_cpu = 1000.0;
  double lowest_enqueue_calls = 1000.0;

  std::vector<FamilyBudget> budgets;
  budgets.reserve(families.size());
  for (const auto& family : families) {
    auto budget = budget_for(family);
    if (budget.product_candidate) {
      product_candidates += 1;
    }
    if (budget.quality_pass) {
      quality_passes += 1;
    }
    if (budget.driver_cpu_pass) {
      cpu_passes += 1;
    }
    best_quality = std::max(best_quality, family.best_quality_alignment_score);
    lowest_median_cpu = std::min(lowest_median_cpu, family.median_driver_cpu_p95);
    lowest_enqueue_calls = std::min(lowest_enqueue_calls, budget.usb_enqueue_calls_per_second);
    budgets.push_back(budget);
  }

  const bool model_passes = product_candidates == 0 && quality_passes == 0 && cpu_passes >= 1;
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.transport-budget-model.v1\",\n"
            << "  \"result\": \"" << (model_passes ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"offline frontier diagnostic; PASS does not mean product readiness\",\n"
            << "  \"thresholds\": {\"quality_alignment_score\": " << kQualityGate
            << ", \"driver_cpu_p95\": " << kDriverCpuP95Gate
            << ", \"lag_jumps_gt_2_frames\": " << kLagJumpGate << "},\n"
            << "  \"summary\": {\"observed_families\": " << families.size()
            << ", \"product_candidate_exists\": "
            << (product_candidates > 0 ? "true" : "false")
            << ", \"quality_passing_families\": " << quality_passes
            << ", \"driver_cpu_passing_families\": " << cpu_passes
            << ", \"best_quality_alignment_score\": " << best_quality
            << ", \"lowest_median_driver_cpu_p95\": " << lowest_median_cpu
            << ", \"lowest_estimated_usb_enqueue_calls_per_second\": "
            << lowest_enqueue_calls << "},\n"
            << "  \"rows\": [\n";
  for (std::size_t index = 0; index < families.size(); ++index) {
    print_family(families[index], budgets[index], index + 1U < families.size());
  }
  std::cout << "  ]\n"
            << "}\n";

  return model_passes ? 0 : 1;
}
