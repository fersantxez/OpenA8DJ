#include "evidence_json.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
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

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return {};
  }
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::filesystem::path repo_root(char** argv) {
  auto root = std::filesystem::absolute(argv[0]).parent_path();
  while (!root.empty() && !std::filesystem::is_regular_file(root / "CMakeLists.txt")) {
    root = root.parent_path();
  }
  if (root.empty() || root.filename() != "audio8djcpp") {
    return "/Users/fer/dev/audio8djcpp";
  }
  return root;
}

bool string_field_is(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string(json, key).value_or("") == expected;
}

bool bool_field_is(std::string_view json, std::string_view key, bool expected) {
  return opena8djcpp::evidence_json::json_bool(json, key).value_or(!expected) == expected;
}

double number_or(std::string_view json, std::string_view key, double fallback) {
  return opena8djcpp::evidence_json::json_number(json, key).value_or(fallback);
}

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

int main(int argc, char** argv) {
  (void)argc;
  const auto evidence = repo_root(argv) / "local-analysis/cpp-offline";
  const auto migration = read_file(evidence / "prepared-transport-migration-gate.json");
  const auto hot_path = read_file(evidence / "hot-path-timing-analysis.json");
  const auto playback_scheduler = read_file(evidence / "playback-scheduler-contract.json");
  const auto playback_scheduler_runtime =
      read_file(evidence / "playback-scheduler-runtime-contract.json");
  const auto product_quality = read_file(evidence / "product-quality-claim-gate.json");
  const auto physical_window = read_file(evidence / "physical-window-readiness-gate.json");

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

  const bool prepared_contracts_present =
      !migration.empty() && string_field_is(migration, "result", "PASS") &&
      bool_field_is(migration, "migration_candidate_supported", true) &&
      bool_field_is(migration, "product_ready", false) &&
      bool_field_is(migration, "branch_promotion_supported", false);
  const double prepared_submit_reduction =
      number_or(migration, "runtime_adapter_stable_usb_submit_reduction_ratio", 0.0);
  const double prepared_usb_submit_calls =
      number_or(migration, "runtime_adapter_stable_usb_submit_calls", 0.0);
  const double prepared_logical_periods =
      number_or(migration, "runtime_adapter_stable_logical_audio_periods", 0.0);
  const double prepared_max_gap_ratio =
      number_or(migration, "prepared_slot_scheduler_max_completion_gap_ratio", 999.0);
  const double prepared_safe_logical_gap =
      number_or(migration, "prepared_slot_scheduler_max_safe_logical_audio_gap_ratio", 999.0);
  const bool hot_path_present = !hot_path.empty() && string_field_is(hot_path, "result", "PASS");
  const double fixed_queue_to_playback_fill_ratio =
      number_or(hot_path, "fixed_queue_to_playback_fill_ratio", -1.0);
  const double fixed_queue_ticks_per_second =
      number_or(hot_path, "fixed_queue_ticks_per_second", -1.0);
  const double prepared_runtime_logical_slots =
      number_or(migration, "prepared_usb_runtime_submit_logical_slots", 0.0);
  const double prepared_runtime_usb_submit_calls =
      number_or(migration, "prepared_usb_runtime_submit_usb_submit_calls", 0.0);
  const double prepared_runtime_slots_per_submit =
      prepared_runtime_usb_submit_calls > 0.0
          ? prepared_runtime_logical_slots / prepared_runtime_usb_submit_calls
          : 0.0;
  const double predicted_prepared_fixed_queue_ticks_per_second =
      fixed_queue_ticks_per_second > 0.0 && prepared_submit_reduction > 0.0
          ? fixed_queue_ticks_per_second / prepared_submit_reduction
          : -1.0;
  const bool resource_model_inputs_complete =
      prepared_contracts_present && hot_path_present && fixed_queue_ticks_per_second > 0.0 &&
      prepared_runtime_logical_slots > 0.0 && prepared_runtime_usb_submit_calls > 0.0;
  const bool offline_resource_superiority_model_pass =
      resource_model_inputs_complete && prepared_submit_reduction >= 8.0 &&
      prepared_runtime_slots_per_submit >= 8.0 &&
      predicted_prepared_fixed_queue_ticks_per_second > 0.0 &&
      predicted_prepared_fixed_queue_ticks_per_second < fixed_queue_ticks_per_second &&
      prepared_max_gap_ratio <= 1.25 && prepared_safe_logical_gap <= 1.0;
  const bool playback_scheduler_model_pass =
      !playback_scheduler.empty() && string_field_is(playback_scheduler, "result", "PASS") &&
      bool_field_is(playback_scheduler, "physical_evidence_present", false) &&
      bool_field_is(playback_scheduler, "product_claim_allowed", false) &&
      number_or(playback_scheduler, "stable_capture_request_submit_calls", 0.0) == 256.0 &&
      number_or(playback_scheduler, "stable_playback_request_submit_calls", 999.0) <= 33.0 &&
      number_or(playback_scheduler, "stable_playback_submit_reduction_ratio", 0.0) >= 8.0 &&
      number_or(playback_scheduler, "stable_total_submit_reduction_ratio", 0.0) > 1.5 &&
      number_or(playback_scheduler, "stable_min_playback_lead_slots", 0.0) >= 4.0 &&
      number_or(playback_scheduler, "stable_max_playback_lead_slots", 999.0) <= 12.0;
  const bool playback_scheduler_runtime_pass =
      !playback_scheduler_runtime.empty() &&
      string_field_is(playback_scheduler_runtime, "result", "PASS") &&
      bool_field_is(playback_scheduler_runtime, "physical_evidence_present", false) &&
      bool_field_is(playback_scheduler_runtime, "product_claim_allowed", false) &&
      number_or(playback_scheduler_runtime, "stable_capture_runtime_submit_calls", 0.0) ==
          256.0 &&
      number_or(playback_scheduler_runtime, "stable_playback_runtime_submit_calls", 999.0) <=
          33.0 &&
      number_or(playback_scheduler_runtime, "stable_playback_logical_slots_submitted", 0.0) ==
          264.0 &&
      number_or(playback_scheduler_runtime, "stable_playback_submit_reduction_ratio", 0.0) >=
          8.0 &&
      number_or(playback_scheduler_runtime, "stable_total_submit_reduction_ratio", 0.0) > 1.5;
  const bool quality_claim_blocked =
      !product_quality.empty() && string_field_is(product_quality, "result", "PASS") &&
      bool_field_is(product_quality, "quality_claim_allowed", false);
  const bool physical_ab_blocked =
      !physical_window.empty() && string_field_is(physical_window, "result", "PASS") &&
      bool_field_is(physical_window, "ready_for_product_physical_ab", false);
  const bool prepared_model_sufficient_for_physical_window =
      prepared_contracts_present && prepared_submit_reduction >= 8.0 &&
      prepared_usb_submit_calls > 0.0 && prepared_logical_periods > 0.0 &&
      prepared_max_gap_ratio <= 1.25 && prepared_safe_logical_gap <= 1.0 &&
      hot_path_present && fixed_queue_to_playback_fill_ratio > 1.0;
  const bool runtime_cpu_superiority_claim_allowed = false;
  const bool model_passes =
      product_candidates == 0 && quality_passes == 0 && cpu_passes >= 1 &&
      playback_scheduler_model_pass && playback_scheduler_runtime_pass;
  const bool playback_scheduler_physical_evidence_present =
      !bool_field_is(playback_scheduler, "physical_evidence_present", false);
  const bool playback_scheduler_product_claim_allowed =
      !bool_field_is(playback_scheduler, "product_claim_allowed", false);

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.transport-budget-model.v1\",\n"
            << "  \"result\": \"" << (model_passes ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"offline frontier diagnostic; PASS does not mean product readiness\",\n"
            << "  \"prepared_runtime_model\": {"
            << "\"contracts_present\": " << (prepared_contracts_present ? "true" : "false")
            << ", \"submit_reduction_ratio\": " << prepared_submit_reduction
            << ", \"usb_submit_calls\": " << prepared_usb_submit_calls
            << ", \"logical_audio_periods\": " << prepared_logical_periods
            << ", \"max_completion_gap_ratio\": " << prepared_max_gap_ratio
            << ", \"max_safe_logical_audio_gap_ratio\": " << prepared_safe_logical_gap
            << ", \"hot_path_present\": " << (hot_path_present ? "true" : "false")
            << ", \"fixed_queue_to_playback_fill_ratio\": "
            << fixed_queue_to_playback_fill_ratio
            << ", \"sufficient_for_physical_window\": "
            << (prepared_model_sufficient_for_physical_window ? "true" : "false")
            << ", \"runtime_cpu_superiority_claim_allowed\": "
            << (runtime_cpu_superiority_claim_allowed ? "true" : "false")
            << ", \"offline_resource_superiority_model_pass\": "
            << (offline_resource_superiority_model_pass ? "true" : "false")
            << ", \"resource_model_inputs_complete\": "
            << (resource_model_inputs_complete ? "true" : "false")
            << ", \"runtime_logical_slots\": " << prepared_runtime_logical_slots
            << ", \"runtime_logical_slots_per_usb_submit\": "
            << prepared_runtime_slots_per_submit
            << ", \"observed_fixed_queue_ticks_per_second\": "
            << fixed_queue_ticks_per_second
            << ", \"predicted_prepared_fixed_queue_ticks_per_second\": "
            << predicted_prepared_fixed_queue_ticks_per_second
            << ", \"predicted_fixed_queue_work_reduction_ratio\": "
            << prepared_submit_reduction
            << ", \"quality_claim_blocked\": " << (quality_claim_blocked ? "true" : "false")
            << ", \"physical_ab_blocked\": " << (physical_ab_blocked ? "true" : "false")
            << ", \"claim_blockers\": [\"same_session_physical_cpu_ab_missing\", "
               "\"prepared_runtime_not_physically_validated\", "
               "\"route_or_product_quality_claim_blocked\"]},\n"
            << "  \"playback_scheduler_model\": {"
            << "\"model_pass\": " << (playback_scheduler_model_pass ? "true" : "false")
            << ", \"stable_capture_request_submit_calls\": "
            << number_or(playback_scheduler, "stable_capture_request_submit_calls", -1.0)
            << ", \"stable_playback_request_submit_calls\": "
            << number_or(playback_scheduler, "stable_playback_request_submit_calls", -1.0)
            << ", \"stable_playback_submit_reduction_ratio\": "
            << number_or(playback_scheduler, "stable_playback_submit_reduction_ratio", -1.0)
            << ", \"stable_total_submit_reduction_ratio\": "
            << number_or(playback_scheduler, "stable_total_submit_reduction_ratio", -1.0)
            << ", \"stable_min_playback_lead_slots\": "
            << number_or(playback_scheduler, "stable_min_playback_lead_slots", -1.0)
            << ", \"stable_max_playback_lead_slots\": "
            << number_or(playback_scheduler, "stable_max_playback_lead_slots", -1.0)
            << ", \"runtime_binding_pass\": "
            << (playback_scheduler_runtime_pass ? "true" : "false")
            << ", \"runtime_capture_submit_calls\": "
            << number_or(playback_scheduler_runtime, "stable_capture_runtime_submit_calls", -1.0)
            << ", \"runtime_playback_submit_calls\": "
            << number_or(playback_scheduler_runtime, "stable_playback_runtime_submit_calls", -1.0)
            << ", \"physical_evidence_present\": "
            << (playback_scheduler_physical_evidence_present ? "true" : "false")
            << ", \"product_claim_allowed\": "
            << (playback_scheduler_product_claim_allowed ? "true" : "false")
            << ", \"next_required_action\": "
               "\"IMPLEMENT_OPT_IN_HAL_PLAYBACK_SCHEDULER_BINDING_THEN_SOURCE_REFERENCE_AB\"},\n"
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
