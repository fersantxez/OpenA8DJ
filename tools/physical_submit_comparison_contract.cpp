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

bool contains(std::string_view text, std::string_view needle) {
  return text.find(needle) != std::string_view::npos;
}

void print_string_array(const char* name, const std::vector<std::string>& values) {
  std::cout << "  \"" << name << "\": [";
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0) {
      std::cout << ", ";
    }
    std::cout << "\"" << values[index] << "\"";
  }
  std::cout << "]";
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const auto compare_source = read_file(root / "tools/physical_run_compare.cpp");
  const auto stream_stats_analyzer = read_file(root / "scripts/analyze-stream-stats.py");
  const auto run_soundcheck = read_file(root / "scripts/run-soundcheck");
  const auto promotion_evaluator = read_file(root / "scripts/evaluate-promotion-readiness.py");

  std::vector<std::string> failures;
  const bool compare_source_present = !compare_source.empty();
  const bool stream_stats_analyzer_present = !stream_stats_analyzer.empty();
  const bool run_soundcheck_present = !run_soundcheck.empty();
  const bool promotion_evaluator_present = !promotion_evaluator.empty();
  if (!compare_source_present) failures.push_back("physical_run_compare_missing");
  if (!stream_stats_analyzer_present) failures.push_back("stream_stats_analyzer_missing");
  if (!run_soundcheck_present) failures.push_back("run_soundcheck_missing");
  if (!promotion_evaluator_present) failures.push_back("promotion_evaluator_missing");

  const bool analyzer_outputs_submit_rates =
      contains(stream_stats_analyzer, "\"capture_transfers_submitted_per_second\"") &&
      contains(stream_stats_analyzer, "\"playback_transfers_submitted_per_second\"") &&
      contains(stream_stats_analyzer, "\"capture_submit_reduction_ratio_vs_logical\"") &&
      contains(stream_stats_analyzer, "\"playback_submit_reduction_ratio_vs_base\"") &&
      contains(stream_stats_analyzer, "\"capture_submit_rate_ratio_to_expected\"") &&
      contains(stream_stats_analyzer, "\"playback_submit_rate_ratio_to_expected\"");
  if (!analyzer_outputs_submit_rates) failures.push_back("analyzer_submit_rates_missing");

  const bool soundcheck_records_submit_counters =
      contains(run_soundcheck, "\"captureTransfersSubmitted\",") &&
      contains(run_soundcheck, "\"playbackTransfersSubmitted\",");
  if (!soundcheck_records_submit_counters) {
    failures.push_back("soundcheck_submit_counter_columns_missing");
  }

  const bool compare_reads_submit_rates =
      contains(compare_source, "capture_submit_calls_per_second") &&
      contains(compare_source, "playback_submit_calls_per_second") &&
      contains(compare_source, "capture_transfers_submitted_per_second") &&
      contains(compare_source, "playback_transfers_submitted_per_second");
  if (!compare_reads_submit_rates) failures.push_back("physical_compare_submit_reads_missing");

  const bool compare_has_legacy_fallback =
      contains(compare_source, "run.capture_submit_calls_per_second = run.capture_transfers_per_second") &&
      contains(compare_source, "run.playback_submit_calls_per_second = run.playback_transfers_per_second");
  if (!compare_has_legacy_fallback) failures.push_back("legacy_submit_fallback_missing");

  const bool same_session_gates_include_submit_rates =
      contains(compare_source, "{\"capture_submit_calls_per_second\", Direction::LessOrEqual") &&
      contains(compare_source, "{\"playback_submit_calls_per_second\", Direction::LessOrEqual");
  if (!same_session_gates_include_submit_rates) {
    failures.push_back("same_session_submit_gates_missing");
  }

  const bool compare_prints_submit_rates =
      contains(compare_source, "\"capture_submit_calls_per_second\"") &&
      contains(compare_source, "\"playback_submit_calls_per_second\"") &&
      contains(compare_source, "\"capture_submit_rate_ratio_to_expected\"") &&
      contains(compare_source, "\"playback_submit_rate_ratio_to_expected\"");
  if (!compare_prints_submit_rates) failures.push_back("physical_compare_submit_output_missing");

  const bool promotion_depends_on_same_session_compare =
      contains(promotion_evaluator, "same_session_compare") &&
      contains(promotion_evaluator, "branch_promotion_supported") &&
      contains(promotion_evaluator, "same_session_mainline_cpp_physical_compare");
  if (!promotion_depends_on_same_session_compare) {
    failures.push_back("promotion_evaluator_same_session_dependency_missing");
  }

  const bool pass = failures.empty();
  std::cout
      << "{\n"
      << "  \"schema\": \"opena8djcpp.physical-submit-comparison-contract.v1\",\n"
      << "  \"safety\": \"offline_source_contract_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
      << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
      << "  \"meaning\": \"PASS means same-session physical comparison must consider capture/playback submit cadence before promotion\",\n"
      << "  \"analyzer_outputs_submit_rates\": "
      << (analyzer_outputs_submit_rates ? "true" : "false") << ",\n"
      << "  \"soundcheck_records_submit_counters\": "
      << (soundcheck_records_submit_counters ? "true" : "false") << ",\n"
      << "  \"compare_reads_submit_rates\": "
      << (compare_reads_submit_rates ? "true" : "false") << ",\n"
      << "  \"compare_has_legacy_fallback\": "
      << (compare_has_legacy_fallback ? "true" : "false") << ",\n"
      << "  \"same_session_gates_include_submit_rates\": "
      << (same_session_gates_include_submit_rates ? "true" : "false") << ",\n"
      << "  \"compare_prints_submit_rates\": "
      << (compare_prints_submit_rates ? "true" : "false") << ",\n"
      << "  \"promotion_depends_on_same_session_compare\": "
      << (promotion_depends_on_same_session_compare ? "true" : "false") << ",\n";
  print_string_array("failures", failures);
  std::cout
      << ",\n"
      << "  \"blocked_claim\": "
         "\"NO_RUNTIME_CPU_OR_RESOURCE_SUPERIORITY_CLAIM_WITHOUT_SAME_SESSION_SUBMIT_CADENCE_COMPARISON\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
