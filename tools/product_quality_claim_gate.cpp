#include "evidence_json.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
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

bool last_string_field_is(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string_last(json, key).value_or("") == expected;
}

bool bool_field_is(std::string_view json, std::string_view key, bool expected) {
  return opena8djcpp::evidence_json::json_bool(json, key).value_or(!expected) == expected;
}

bool bool_field_present_and_is(std::string_view json, std::string_view key, bool expected) {
  const auto value = opena8djcpp::evidence_json::json_bool(json, key);
  return value.has_value() && *value == expected;
}

bool string_array_has(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string_array_contains(json, key, expected);
}

std::string string_or(std::string_view json, std::string_view key, std::string_view fallback) {
  return opena8djcpp::evidence_json::json_string(json, key).value_or(std::string(fallback));
}

double number_or(std::string_view json, std::string_view key, double fallback) {
  return opena8djcpp::evidence_json::json_number(json, key).value_or(fallback);
}

bool finite_gt(double value, double threshold) {
  return std::isfinite(value) && value > threshold;
}

void print_string_array(const char* name, const std::vector<std::string>& values) {
  std::cout << "  \"" << name << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      std::cout << ", ";
    }
    std::cout << "\"" << values[index] << "\"";
  }
  std::cout << "],\n";
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto evidence = repo_root(argv) / "local-analysis/cpp-offline";

  const auto soundcheck = read_file(evidence / "soundcheck-wav-quality.json");
  const auto physical_compare = read_file(evidence / "physical-run-product-superiority.json");
  const auto capture_route = read_file(evidence / "capture-route-health-gate.json");
  const auto tone = read_file(evidence / "audiophile-tone-gate.json");
  const auto promotion = read_file(evidence / "promotion-readiness-offline-check.json");

  const bool evidence_present = !soundcheck.empty() && !physical_compare.empty() &&
                                !capture_route.empty() && !tone.empty() && !promotion.empty();

  const bool soundcheck_analyzer_only =
      string_field_is(soundcheck, "readiness_claim", "ANALYZER_ONLY_NOT_PRODUCT_READINESS");
  const bool audiophile_analyzers_pass =
      bool_field_present_and_is(physical_compare,
                                "audiophile_wav_analysis_required_before_promotion",
                                false);
  const bool real_music_superiority =
      string_field_is(physical_compare, "result", "PASS") &&
      bool_field_is(physical_compare, "branch_promotion_supported", true);
  const bool route_valid = bool_field_is(capture_route, "measurement_valid_for_promotion", true);
  const bool direct_usb_failed_after_clean_payload =
      bool_field_is(capture_route, "direct_usb_capture_failed_after_clean_payload", true) &&
      string_array_has(capture_route, "promotion_blockers",
                       "direct_usb_capture_failed_after_clean_payload");
  const bool tone_pass = string_field_is(tone, "result", "PASS");
  const bool tone_current_valid = bool_field_is(tone, "physical_measurement_valid_for_promotion", true);
  const bool tone_historical_floor_met =
      bool_field_present_and_is(tone, "candidate_meets_minimum_historical_tone_floor", true);
  const bool tone_preferred_floor_met =
      bool_field_present_and_is(tone, "candidate_meets_preferred_historical_tone_floor", true);
  const bool promotion_allowed = last_string_field_is(promotion, "result", "PASS") &&
                                 bool_field_is(promotion, "branch_promotion_allowed", true);
  const std::string residual_classification =
      string_or(physical_compare, "residual_classification", "");
  const std::string residual_timing_status =
      string_or(physical_compare, "residual_timing_status", "");
  const double residual_timing_explain_db =
      number_or(physical_compare, "residual_timing_explain_db", -1.0);
  const double native_lag_jumps =
      number_or(physical_compare, "native_lag_jumps_gt_2_frames", -1.0);
  const double audiophile_cpp_delay_p95 =
      number_or(physical_compare, "audiophile_cpp_delay_p95_frames", -1.0);
  const double audiophile_python_delay_p95 =
      number_or(physical_compare, "audiophile_python_delay_p95_frames", -1.0);
  const bool timing_instability_blocks_quality_claim =
      residual_classification == "timing_instability_dominant" ||
      residual_timing_status == "timing_explains_material_residual" ||
      finite_gt(residual_timing_explain_db, 3.0) || finite_gt(native_lag_jumps, 0.0) ||
      finite_gt(audiophile_cpp_delay_p95, 2.0) ||
      finite_gt(audiophile_python_delay_p95, 2.0);

  const bool quality_claim_allowed = evidence_present && real_music_superiority &&
                                     audiophile_analyzers_pass && route_valid && tone_pass &&
                                     tone_current_valid && tone_historical_floor_met &&
                                     tone_preferred_floor_met && promotion_allowed &&
                                     !soundcheck_analyzer_only &&
                                     !timing_instability_blocks_quality_claim;

  std::vector<std::string> blockers;
  if (!evidence_present) {
    blockers.push_back("required_quality_evidence_missing");
  }
  if (soundcheck_analyzer_only) {
    blockers.push_back("latest_soundcheck_is_analyzer_only");
  }
  if (!real_music_superiority) {
    blockers.push_back("real_music_same_session_superiority_missing");
  }
  if (!audiophile_analyzers_pass) {
    blockers.push_back("same_session_audiophile_wav_analyzers_missing_or_failing");
  }
  if (!route_valid) {
    blockers.push_back("capture_route_not_valid_for_promotion");
  }
  if (direct_usb_failed_after_clean_payload) {
    blockers.push_back("direct_usb_capture_failed_after_clean_payload");
  }
  if (!tone_pass) {
    blockers.push_back("audiophile_tone_gate_not_passed");
  }
  if (!tone_current_valid) {
    blockers.push_back("audiophile_tone_not_current_promotion_measurement");
  }
  if (!tone_historical_floor_met) {
    blockers.push_back("audiophile_tone_historical_floor_not_met");
  }
  if (!tone_preferred_floor_met) {
    blockers.push_back("audiophile_tone_preferred_floor_not_met");
  }
  if (!promotion_allowed) {
    blockers.push_back("branch_promotion_not_allowed");
  }
  if (timing_instability_blocks_quality_claim) {
    blockers.push_back("timing_instability_dominant_or_delay_unstable");
  }

  const bool guard_pass = evidence_present && !quality_claim_allowed &&
                          (direct_usb_failed_after_clean_payload || blockers.size() >= 4U);

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.product-quality-claim-gate.v1\",\n"
            << "  \"safety\": \"offline_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (guard_pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means the quality claim guard is active and product quality remains blocked\",\n"
            << "  \"evidence_present\": " << (evidence_present ? "true" : "false") << ",\n"
            << "  \"quality_claim_allowed\": " << (quality_claim_allowed ? "true" : "false")
            << ",\n"
            << "  \"soundcheck_analyzer_only\": " << (soundcheck_analyzer_only ? "true" : "false")
            << ",\n"
            << "  \"real_music_superiority\": " << (real_music_superiority ? "true" : "false")
            << ",\n"
            << "  \"same_session_audiophile_wav_analyzers_pass\": "
            << (audiophile_analyzers_pass ? "true" : "false") << ",\n"
            << "  \"capture_route_valid_for_promotion\": " << (route_valid ? "true" : "false")
            << ",\n"
            << "  \"direct_usb_capture_failed_after_clean_payload\": "
            << (direct_usb_failed_after_clean_payload ? "true" : "false") << ",\n"
            << "  \"audiophile_tone_pass\": " << (tone_pass ? "true" : "false") << ",\n"
            << "  \"audiophile_tone_current_promotion_measurement\": "
            << (tone_current_valid ? "true" : "false") << ",\n"
            << "  \"audiophile_tone_historical_floor_met\": "
            << (tone_historical_floor_met ? "true" : "false") << ",\n"
            << "  \"audiophile_tone_preferred_floor_met\": "
            << (tone_preferred_floor_met ? "true" : "false") << ",\n"
            << "  \"branch_promotion_allowed\": " << (promotion_allowed ? "true" : "false")
            << ",\n"
            << "  \"timing_instability_blocks_quality_claim\": "
            << (timing_instability_blocks_quality_claim ? "true" : "false") << ",\n"
            << "  \"latest_residual_classification\": \"" << residual_classification << "\",\n"
            << "  \"latest_residual_timing_status\": \"" << residual_timing_status << "\",\n"
            << "  \"latest_residual_timing_explain_db\": " << residual_timing_explain_db
            << ",\n"
            << "  \"latest_native_lag_jumps_gt_2_frames\": " << native_lag_jumps << ",\n"
            << "  \"latest_audiophile_cpp_delay_p95_frames\": " << audiophile_cpp_delay_p95
            << ",\n"
            << "  \"latest_audiophile_python_delay_p95_frames\": "
            << audiophile_python_delay_p95 << ",\n"
            << "  \"latest_soundcheck_quality_alignment_score\": "
            << number_or(soundcheck, "quality_alignment_score", -1.0) << ",\n"
            << "  \"latest_soundcheck_snr_floor_db\": "
            << std::min(number_or(soundcheck, "left_snr_db", -999.0),
                        number_or(soundcheck, "right_snr_db", -999.0))
            << ",\n";
  print_string_array("quality_claim_blockers", blockers);
  std::cout
      << "  \"blocked_claim\": "
         "\"NO_AUDIOPHILE_QUALITY_CLAIM_UNTIL_REAL_MUSIC_ANALYZERS_TONE_ROUTE_AND_SAME_SESSION_PROMOTION_PASS\"\n"
      << "}\n";
  return guard_pass ? 0 : 1;
}
