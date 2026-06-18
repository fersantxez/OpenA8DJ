#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kQualityGate = 0.98;
constexpr double kSnrGate = 35.0;
constexpr double kCleanAlignmentGate = 0.999999;
constexpr double kCleanSnrGate = 120.0;

struct DirectRun {
  std::filesystem::path dir;
  bool has_required_artifacts = false;
  bool has_evidence_time = false;
  std::filesystem::file_time_type evidence_time{};

  double written_alignment = std::numeric_limits<double>::quiet_NaN();
  double written_left_snr = std::numeric_limits<double>::quiet_NaN();
  double written_right_snr = std::numeric_limits<double>::quiet_NaN();
  double consumed_alignment = std::numeric_limits<double>::quiet_NaN();
  double consumed_left_snr = std::numeric_limits<double>::quiet_NaN();
  double consumed_right_snr = std::numeric_limits<double>::quiet_NaN();
  double written_consumed_alignment = std::numeric_limits<double>::quiet_NaN();
  double usb_alignment = std::numeric_limits<double>::quiet_NaN();
  double usb_left_snr = std::numeric_limits<double>::quiet_NaN();
  double usb_right_snr = std::numeric_limits<double>::quiet_NaN();
  double usb_check_errors = std::numeric_limits<double>::quiet_NaN();
  double usb_panic_flags = std::numeric_limits<double>::quiet_NaN();

  double capture_quality = std::numeric_limits<double>::quiet_NaN();
  double capture_left_snr = std::numeric_limits<double>::quiet_NaN();
  double capture_right_snr = std::numeric_limits<double>::quiet_NaN();
  double capture_mid_ratio = std::numeric_limits<double>::quiet_NaN();
  double capture_high_ratio = std::numeric_limits<double>::quiet_NaN();
  double capture_lag_jumps = std::numeric_limits<double>::quiet_NaN();
  double audiophile_snr_floor_db = std::numeric_limits<double>::quiet_NaN();
  double audiophile_mid_coherence_floor = std::numeric_limits<double>::quiet_NaN();
  double audiophile_delay_p95_frames = std::numeric_limits<double>::quiet_NaN();
  double audiophile_alignment_score = std::numeric_limits<double>::quiet_NaN();
  double timing_explain_db = std::numeric_limits<double>::quiet_NaN();
  double routing_matrix_explain_db = std::numeric_limits<double>::quiet_NaN();
  std::string residual_classification;
  std::string audiophile_result;

  double failure_drift_ppm = std::numeric_limits<double>::quiet_NaN();
  double failure_matrix_improve_db = std::numeric_limits<double>::quiet_NaN();
  double failure_polynomial_improve_db = std::numeric_limits<double>::quiet_NaN();
  double lti_mid_coherence = std::numeric_limits<double>::quiet_NaN();
  double lti_snr_delta_db = std::numeric_limits<double>::quiet_NaN();
  bool physical_routing_pass = false;
  double max_wrong_source_leakage_db = std::numeric_limits<double>::quiet_NaN();

  std::string attribution;
  bool internal_clean = false;
  bool capture_failed = false;
};

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

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return {};
  }
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool finite(double value) {
  return std::isfinite(value);
}

std::optional<double> parse_double(const std::string& text) {
  try {
    std::size_t consumed = 0;
    const double value = std::stod(text, &consumed);
    return consumed > 0U && finite(value) ? std::optional<double>(value) : std::nullopt;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

std::optional<double> key_value_number(const std::string& text, const std::string& key) {
  const std::string needle = key + "=";
  const auto pos = text.find(needle);
  if (pos == std::string::npos) {
    return std::nullopt;
  }
  const auto start = pos + needle.size();
  auto end = start;
  while (end < text.size() && !std::isspace(static_cast<unsigned char>(text[end]))) {
    ++end;
  }
  return parse_double(text.substr(start, end - start));
}

std::optional<double> json_number(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  if (json.compare(start, 4U, "null") == 0) {
    return std::nullopt;
  }
  std::size_t end = start;
  while (end < json.size()) {
    const char c = json[end];
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.' ||
          c == 'e' || c == 'E')) {
      break;
    }
    ++end;
  }
  if (end == start) {
    return std::nullopt;
  }
  return parse_double(json.substr(start, end - start));
}

std::optional<std::string> json_string(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  if (start >= json.size() || json[start] != '"') {
    return std::nullopt;
  }
  ++start;
  std::string out;
  bool escaped = false;
  for (std::size_t i = start; i < json.size(); ++i) {
    const char c = json[i];
    if (escaped) {
      out.push_back(c);
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else if (c == '"') {
      return out;
    } else {
      out.push_back(c);
    }
  }
  return std::nullopt;
}

std::optional<bool> json_bool(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
    ++start;
  }
  if (json.compare(start, 4U, "true") == 0) {
    return true;
  }
  if (json.compare(start, 5U, "false") == 0) {
    return false;
  }
  return std::nullopt;
}

std::string json_slice_from_key(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos) {
    return {};
  }
  return json.substr(key_pos);
}

double number_or_nan(std::optional<double> value) {
  return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

std::string json_escape(const std::string& input) {
  std::string out;
  for (const char c : input) {
    if (c == '"' || c == '\\') {
      out.push_back('\\');
    }
    out.push_back(c);
  }
  return out;
}

void print_number(double value) {
  if (finite(value)) {
    std::cout << value;
  } else {
    std::cout << "null";
  }
}

void update_evidence_time(DirectRun& run, const std::filesystem::path& path) {
  std::error_code error;
  const auto time = std::filesystem::last_write_time(path, error);
  if (error) {
    return;
  }
  if (!run.has_evidence_time || time > run.evidence_time) {
    run.evidence_time = time;
    run.has_evidence_time = true;
  }
}

DirectRun read_run(const std::filesystem::path& dir) {
  DirectRun run{};
  run.dir = dir;

  const auto diagnostics = read_file(dir / "driver-diagnostics-analysis.txt");
  const auto native = read_file(dir / "native-wav-quality.json");
  const auto metrics = read_file(dir / "metrics.json");
  const auto failure = read_file(dir / "failure-modes.json");
  const auto lti = read_file(dir / "lti-transfer-quality.json");
  const auto tone_matrix = read_file(dir / "tone-matrix.json");
  const auto audiophile = read_file(dir / "audiophile-wav-analysis-maxlag6.json");
  run.has_required_artifacts = !diagnostics.empty() && (!native.empty() || !metrics.empty());
  for (const auto& path : {dir / "driver-diagnostics-analysis.txt", dir / "native-wav-quality.json",
                           dir / "metrics.json", dir / "failure-modes.json",
                           dir / "lti-transfer-quality.json", dir / "tone-matrix.json",
                           dir / "audiophile-wav-analysis-maxlag6.json"}) {
    if (std::filesystem::is_regular_file(path)) {
      update_evidence_time(run, path);
    }
  }

  run.written_alignment = number_or_nan(key_value_number(diagnostics, "written_alignment_score"));
  run.written_left_snr = number_or_nan(key_value_number(diagnostics, "written_left_snr_db"));
  run.written_right_snr = number_or_nan(key_value_number(diagnostics, "written_right_snr_db"));
  run.consumed_alignment = number_or_nan(key_value_number(diagnostics, "consumed_alignment_score"));
  run.consumed_left_snr = number_or_nan(key_value_number(diagnostics, "consumed_left_snr_db"));
  run.consumed_right_snr = number_or_nan(key_value_number(diagnostics, "consumed_right_snr_db"));
  run.written_consumed_alignment =
      number_or_nan(key_value_number(diagnostics, "written_consumed_alignment_score"));
  run.usb_alignment = number_or_nan(key_value_number(diagnostics, "usb_alignment_score"));
  run.usb_left_snr = number_or_nan(key_value_number(diagnostics, "usb_left_snr_db"));
  run.usb_right_snr = number_or_nan(key_value_number(diagnostics, "usb_right_snr_db"));
  run.usb_check_errors = number_or_nan(key_value_number(diagnostics, "usb_check_errors"));
  run.usb_panic_flags = number_or_nan(key_value_number(diagnostics, "usb_panic_flags"));

  const auto& capture_json = native.empty() ? metrics : native;
  run.capture_quality = number_or_nan(json_number(capture_json, "quality_alignment_score"));
  run.capture_left_snr = number_or_nan(json_number(capture_json, "left_snr_db"));
  run.capture_right_snr = number_or_nan(json_number(capture_json, "right_snr_db"));
  run.capture_mid_ratio = number_or_nan(json_number(capture_json, "mid_band_residual_ratio"));
  run.capture_high_ratio = number_or_nan(json_number(capture_json, "high_band_residual_ratio"));
  run.capture_lag_jumps = number_or_nan(json_number(capture_json, "lag_jumps_gt_2_frames"));
  run.audiophile_result = json_string(audiophile, "result").value_or("missing");
  run.audiophile_alignment_score =
      number_or_nan(json_number(json_slice_from_key(audiophile, "alignment"), "score"));
  const auto left_audiophile = json_slice_from_key(audiophile, "left");
  const auto right_audiophile = json_slice_from_key(audiophile, "right");
  run.audiophile_snr_floor_db =
      std::min(number_or_nan(json_number(left_audiophile, "snr_db")),
               number_or_nan(json_number(right_audiophile, "snr_db")));
  run.audiophile_mid_coherence_floor =
      std::min(number_or_nan(json_number(left_audiophile, "coherence_mid_active_mean")),
               number_or_nan(json_number(right_audiophile, "coherence_mid_active_mean")));
  run.audiophile_delay_p95_frames =
      number_or_nan(json_number(json_slice_from_key(audiophile, "delay_windows"),
                                "p95_abs_frames"));
  run.timing_explain_db = number_or_nan(json_number(capture_json, "timing_explain_db"));
  run.routing_matrix_explain_db = number_or_nan(json_number(capture_json, "routing_matrix_explain_db"));
  run.residual_classification =
      json_string(capture_json, "classification").value_or("missing_or_not_reanalyzed");

  run.failure_drift_ppm = number_or_nan(json_number(failure, "drift_ppm"));
  run.failure_matrix_improve_db = number_or_nan(json_number(failure, "snr_improvement_db"));
  const auto first_improvement = failure.find("\"polynomial_fit\"");
  if (first_improvement != std::string::npos) {
    run.failure_polynomial_improve_db =
        number_or_nan(json_number(failure.substr(first_improvement), "snr_improvement_db"));
  }
  run.lti_mid_coherence = number_or_nan(json_number(lti, "min_mid_coherence"));
  run.lti_snr_delta_db = number_or_nan(json_number(lti, "min_lti_snr_delta_db"));
  run.physical_routing_pass = json_string(tone_matrix, "result").value_or("missing") == "PASS";
  run.max_wrong_source_leakage_db =
      number_or_nan(json_number(tone_matrix, "max_wrong_source_leakage_db"));

  const auto min_written_snr = std::min(run.written_left_snr, run.written_right_snr);
  const auto min_consumed_snr = std::min(run.consumed_left_snr, run.consumed_right_snr);
  const auto min_usb_snr = std::min(run.usb_left_snr, run.usb_right_snr);
  run.internal_clean = run.has_required_artifacts && run.written_alignment >= kCleanAlignmentGate &&
                       run.consumed_alignment >= kCleanAlignmentGate &&
                       run.written_consumed_alignment >= kCleanAlignmentGate &&
                       run.usb_alignment >= kCleanAlignmentGate &&
                       min_written_snr >= kCleanSnrGate && min_consumed_snr >= kCleanSnrGate &&
                       min_usb_snr >= kCleanSnrGate && run.usb_check_errors == 0.0 &&
                       run.usb_panic_flags == 0.0;
  const auto snr_floor = std::min(run.capture_left_snr, run.capture_right_snr);
  const bool audiophile_claim_blocked =
      !audiophile.empty() &&
      (run.audiophile_result == "FAIL" ||
       json_bool(audiophile, "product_claim_allowed").value_or(false) == false);
  run.capture_failed =
      finite(run.capture_quality) && finite(snr_floor) &&
      (run.capture_quality < kQualityGate || snr_floor < kSnrGate ||
       (finite(run.capture_mid_ratio) && run.capture_mid_ratio > 1.0) ||
       (finite(run.capture_high_ratio) && run.capture_high_ratio > 1.0) ||
       audiophile_claim_blocked);
  if (run.internal_clean && run.capture_failed) {
    run.attribution = "post_usb_device_analog_or_capture_route_dominant";
  } else if (!run.internal_clean) {
    run.attribution = "internal_usb_payload_not_clean";
  } else {
    run.attribution = "not_enough_evidence_or_capture_passed";
  }
  return run;
}

const DirectRun* latest_attribution_run(const std::vector<DirectRun>& runs) {
  const DirectRun* selected = nullptr;
  for (const auto& run : runs) {
    if (!run.has_required_artifacts) {
      continue;
    }
    if (selected == nullptr ||
        (run.has_evidence_time && !selected->has_evidence_time) ||
        (run.has_evidence_time && selected->has_evidence_time &&
         run.evidence_time > selected->evidence_time) ||
        (run.has_evidence_time == selected->has_evidence_time &&
         run.evidence_time == selected->evidence_time && run.dir.string() > selected->dir.string())) {
      selected = &run;
    }
  }
  return selected;
}

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  const auto direct_root = root / "local-analysis/direct-usb-soundcheck";
  std::vector<DirectRun> runs;
  if (std::filesystem::is_directory(direct_root)) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(direct_root)) {
      if (!entry.is_regular_file() || entry.path().filename() != "driver-diagnostics-analysis.txt") {
        continue;
      }
      runs.push_back(read_run(entry.path().parent_path()));
    }
  }
  std::sort(runs.begin(), runs.end(), [](const DirectRun& left, const DirectRun& right) {
    return left.dir.string() < right.dir.string();
  });

  std::uint32_t required_artifact_runs = 0;
  std::uint32_t internal_clean_runs = 0;
  std::uint32_t capture_failed_after_clean_runs = 0;
  std::uint32_t product_candidates = 0;
  std::uint32_t physical_routing_pass_runs = 0;
  for (const auto& run : runs) {
    required_artifact_runs += run.has_required_artifacts ? 1U : 0U;
    internal_clean_runs += run.internal_clean ? 1U : 0U;
    capture_failed_after_clean_runs += (run.internal_clean && run.capture_failed) ? 1U : 0U;
    physical_routing_pass_runs += run.physical_routing_pass ? 1U : 0U;
    const auto snr_floor = std::min(run.capture_left_snr, run.capture_right_snr);
    product_candidates +=
        (run.internal_clean && run.capture_quality >= kQualityGate && snr_floor >= kSnrGate) ? 1U
                                                                                              : 0U;
  }
  const auto* latest = latest_attribution_run(runs);
  const bool pass = latest != nullptr && latest->internal_clean && latest->capture_failed;

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.direct-usb-path-attribution.v1\",\n"
            << "  \"safety\": \"offline_existing_direct_usb_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means latest direct USB diagnostics cleanly separate internal payload from failed physical capture; not product readiness\",\n"
            << "  \"run_count\": " << runs.size() << ",\n"
            << "  \"required_artifact_runs\": " << required_artifact_runs << ",\n"
            << "  \"internal_clean_runs\": " << internal_clean_runs << ",\n"
            << "  \"capture_failed_after_clean_runs\": " << capture_failed_after_clean_runs << ",\n"
            << "  \"physical_routing_pass_runs\": " << physical_routing_pass_runs << ",\n"
            << "  \"product_candidate_runs\": " << product_candidates << ",\n"
            << "  \"latest_run\": ";
  if (latest != nullptr) {
    std::cout << "{\"path\": \"" << json_escape(latest->dir.string())
              << "\", \"attribution\": \"" << latest->attribution
              << "\", \"internal_clean\": " << (latest->internal_clean ? "true" : "false")
              << ", \"capture_failed\": " << (latest->capture_failed ? "true" : "false")
              << ", \"capture_quality_alignment_score\": ";
    print_number(latest->capture_quality);
    std::cout << ", \"capture_snr_floor_db\": ";
    print_number(std::min(latest->capture_left_snr, latest->capture_right_snr));
    std::cout << ", \"capture_mid_band_residual_ratio\": ";
    print_number(latest->capture_mid_ratio);
    std::cout << ", \"capture_high_band_residual_ratio\": ";
    print_number(latest->capture_high_ratio);
    std::cout << ", \"native_lag_jumps_gt_2_frames\": ";
    print_number(latest->capture_lag_jumps);
    std::cout << ", \"audiophile_wav_analysis_result\": \""
              << json_escape(latest->audiophile_result) << "\"";
    std::cout << ", \"audiophile_alignment_score\": ";
    print_number(latest->audiophile_alignment_score);
    std::cout << ", \"audiophile_snr_floor_db\": ";
    print_number(latest->audiophile_snr_floor_db);
    std::cout << ", \"audiophile_mid_coherence_floor\": ";
    print_number(latest->audiophile_mid_coherence_floor);
    std::cout << ", \"audiophile_delay_p95_frames\": ";
    print_number(latest->audiophile_delay_p95_frames);
    std::cout << ", \"usb_alignment_score\": ";
    print_number(latest->usb_alignment);
    std::cout << ", \"usb_snr_floor_db\": ";
    print_number(std::min(latest->usb_left_snr, latest->usb_right_snr));
    std::cout << ", \"usb_check_errors\": ";
    print_number(latest->usb_check_errors);
    std::cout << ", \"usb_panic_flags\": ";
    print_number(latest->usb_panic_flags);
    std::cout << ", \"timing_explain_db\": ";
    print_number(latest->timing_explain_db);
    std::cout << ", \"routing_matrix_explain_db\": ";
    print_number(latest->routing_matrix_explain_db);
    std::cout << ", \"failure_drift_ppm\": ";
    print_number(latest->failure_drift_ppm);
    std::cout << ", \"failure_matrix_snr_improvement_db\": ";
    print_number(latest->failure_matrix_improve_db);
    std::cout << ", \"failure_polynomial_snr_improvement_db\": ";
    print_number(latest->failure_polynomial_improve_db);
    std::cout << ", \"lti_mid_coherence\": ";
    print_number(latest->lti_mid_coherence);
    std::cout << ", \"lti_snr_delta_db\": ";
    print_number(latest->lti_snr_delta_db);
    std::cout << ", \"physical_routing_pass\": "
              << (latest->physical_routing_pass ? "true" : "false");
    std::cout << ", \"max_wrong_source_leakage_db\": ";
    print_number(latest->max_wrong_source_leakage_db);
    std::cout << ", \"residual_classification\": \""
              << json_escape(latest->residual_classification) << "\"},\n";
  } else {
    std::cout << "null,\n";
  }
  std::cout << "  \"readiness_claim\": \"DIAGNOSTIC_ONLY_DIRECT_USB_PHYSICAL_CAPTURE_STILL_FAILS\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
