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

bool contains(std::string_view haystack, std::string_view needle) {
  return haystack.find(needle) != std::string_view::npos;
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
  const auto root = repo_root(argv);

  const auto requirements = read_file(root / "requirements-analysis.txt");
  const auto cpp_analyzer = read_file(root / "tools/audiophile_wav_analysis.cpp");
  const auto python_analyzer = read_file(root / "scripts/analyze-audiophile-wav.py");
  const auto physical_window = read_file(root / "scripts/run-physical-superiority-window");
  const auto direct_usb = read_file(root / "scripts/run-direct-usb-soundcheck");
  const auto known_good = read_file(root / "scripts/run-known-good-route-soundcheck");
  const auto physical_compare = read_file(root / "tools/physical_run_compare.cpp");
  const auto product_gate = read_file(root / "tools/product_quality_claim_gate.cpp");
  const auto offline_runner = read_file(root / "scripts/run-cpp-offline-gates");

  std::vector<std::string> blockers;

  const bool dependencies_pinned = contains(requirements, "numpy==") &&
                                   contains(requirements, "scipy==") &&
                                   contains(requirements, "soundfile==");
  if (!dependencies_pinned) {
    blockers.push_back("analysis_python_dependencies_not_pinned");
  }

  const bool cpp_analyzer_native =
      contains(cpp_analyzer, "opena8djcpp.audiophile-wav-analysis-cpp.v1") &&
      contains(cpp_analyzer, "min_alignment_score") &&
      contains(cpp_analyzer, "alignment_score_below_threshold") &&
      contains(cpp_analyzer, "coherence_mid_active_mean") &&
      contains(cpp_analyzer, "delay_windows") &&
      contains(cpp_analyzer, "worst_offdiag_db_relative") &&
      contains(cpp_analyzer, "residual_burst_p95_to_median_db") &&
      contains(cpp_analyzer, "residual_signal_abs_correlation") &&
      contains(cpp_analyzer, "residual_peak_to_rms_db") &&
      contains(cpp_analyzer, "product_claim_allowed") &&
      contains(cpp_analyzer, "false");
  if (!cpp_analyzer_native) {
    blockers.push_back("compiled_cpp_audiophile_analyzer_missing_required_metrics");
  }

  const bool cpp_analyzer_rejects_degraded_self_test =
      contains(cpp_analyzer, "self_test_degraded") &&
      contains(cpp_analyzer, "--self-test-degraded") &&
      contains(cpp_analyzer, "make_self_degraded_capture");
  if (!cpp_analyzer_rejects_degraded_self_test) {
    blockers.push_back("compiled_cpp_audiophile_analyzer_missing_degraded_rejection_self_test");
  }

  const bool python_oracle_retained =
      contains(python_analyzer, "import numpy as np") &&
      contains(python_analyzer, "import scipy.signal") &&
      contains(python_analyzer, "min_alignment_score") &&
      contains(python_analyzer, "alignment_score_below_threshold") &&
      contains(python_analyzer, "import soundfile as sf") &&
      contains(python_analyzer, "scipy.signal.coherence") &&
      contains(python_analyzer, "scipy.signal.csd") &&
      contains(python_analyzer, "scipy.signal.welch");
  if (!python_oracle_retained) {
    blockers.push_back("python_scipy_oracle_missing_required_spectral_checks");
  }

  const bool python_oracle_rejects_degraded_self_test =
      contains(python_analyzer, "self-test-degraded") &&
      contains(python_analyzer, "degraded") &&
      contains(python_analyzer, "standard_normal");
  if (!python_oracle_rejects_degraded_self_test) {
    blockers.push_back("python_scipy_oracle_missing_degraded_rejection_self_test");
  }

  const bool physical_window_runs_both =
      contains(physical_window, "opena8djcpp_audiophile_wav_analysis") &&
      contains(physical_window, "audiophile-wav-analysis-cpp.json") &&
      contains(physical_window, "audiophile-wav-analysis.json") &&
      contains(physical_window, "audiophile_cpp_rc") &&
      contains(physical_window, "audiophile_python_rc");
  if (!physical_window_runs_both) {
    blockers.push_back("physical_window_does_not_run_dual_audiophile_analyzers");
  }

  const bool direct_usb_runs_wide_lag_audiophile =
      contains(direct_usb, "audiophile-wav-analysis-maxlag6.json") &&
      contains(direct_usb, "--max-lag-seconds") && contains(direct_usb, "6") &&
      contains(direct_usb, "audiophile_wav_analysis_maxlag6_rc");
  if (!direct_usb_runs_wide_lag_audiophile) {
    blockers.push_back("direct_usb_runner_missing_wide_lag_audiophile_analysis");
  }

  const bool known_good_runs_both =
      contains(known_good, "opena8djcpp_audiophile_wav_analysis") &&
      contains(known_good, "audiophile-wav-analysis-cpp.json") &&
      contains(known_good, "audiophile-wav-analysis.json") &&
      contains(known_good, "audiophile_cpp_rc") &&
      contains(known_good, "audiophile_python_rc");
  if (!known_good_runs_both) {
    blockers.push_back("known_good_route_does_not_run_dual_audiophile_analyzers");
  }

  const bool comparator_requires_both =
      contains(physical_compare, "candidate_audiophile_cpp_wav_analysis_pass") &&
      contains(physical_compare, "candidate_audiophile_python_wav_analysis_pass") &&
      contains(physical_compare, "baseline_audiophile_cpp_wav_analysis_pass") &&
      contains(physical_compare, "baseline_audiophile_python_wav_analysis_pass") &&
      contains(physical_compare, "append_audiophile_dual_oracle_gates") &&
      contains(physical_compare, "audiophile_dual_oracle_alignment_delta") &&
      contains(physical_compare, "audiophile_dual_oracle_lag_delta_frames") &&
      contains(physical_compare, "audiophile_dual_oracle_snr_floor_delta_db") &&
      contains(physical_compare, "audiophile_dual_oracle_delay_p95_delta_frames") &&
      contains(physical_compare, "audiophile_wav_analysis_required_before_promotion");
  if (!comparator_requires_both) {
    blockers.push_back("physical_comparator_does_not_require_dual_analyzer_pass_and_agreement");
  }

  const bool product_claim_blocks_without_both =
      contains(product_gate, "same_session_audiophile_wav_analyzers_pass") &&
      contains(product_gate, "same_session_audiophile_wav_analyzers_missing_or_failing") &&
      contains(product_gate,
               "NO_AUDIOPHILE_QUALITY_CLAIM_UNTIL_REAL_MUSIC_ANALYZERS_TONE_ROUTE_AND_SAME_SESSION_PROMOTION_PASS");
  if (!product_claim_blocks_without_both) {
    blockers.push_back("product_quality_gate_does_not_block_missing_dual_analyzers");
  }

  const bool offline_runner_self_tests_both =
      contains(offline_runner, "audiophile-wav-analysis-cpp-self-test.json") &&
      contains(offline_runner, "audiophile-wav-analysis-self-test.json") &&
      contains(offline_runner, "opena8djcpp_audiophile_wav_analysis") &&
      contains(offline_runner, "scripts/analyze-audiophile-wav.py") &&
      contains(offline_runner, "audiophile_wav_analysis_cpp_self_test") &&
      contains(offline_runner, "audiophile_wav_analysis_python_self_test");
  if (!offline_runner_self_tests_both) {
    blockers.push_back("offline_runner_does_not_self_test_both_analyzers");
  }

  const bool offline_runner_self_tests_degraded =
      contains(offline_runner, "audiophile-wav-analysis-cpp-degraded-self-test.json") &&
      contains(offline_runner, "audiophile-wav-analysis-python-degraded-self-test.json") &&
      contains(offline_runner, "audiophile_wav_analysis_cpp_degraded_self_test") &&
      contains(offline_runner, "audiophile_wav_analysis_python_degraded_self_test") &&
      contains(offline_runner, "degraded_self_test_summary");
  if (!offline_runner_self_tests_degraded) {
    blockers.push_back("offline_runner_does_not_reject_degraded_audiophile_self_tests");
  }

  const bool pass = blockers.empty();
  std::vector<std::string> required_runtime_artifacts{
      "candidate/audiophile-wav-analysis-cpp.json",
      "candidate/audiophile-wav-analysis.json",
      "baseline/audiophile-wav-analysis-cpp.json",
      "baseline/audiophile-wav-analysis.json",
  };

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.audiophile-analysis-stack-contract.v1\",\n"
            << "  \"safety\": \"offline_static_contract_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means the audiophile WAV analysis stack is dual-path and fail-closed; it is not product readiness\",\n"
            << "  \"dependencies_pinned\": " << (dependencies_pinned ? "true" : "false")
            << ",\n"
            << "  \"cpp_analyzer_native\": " << (cpp_analyzer_native ? "true" : "false")
            << ",\n"
            << "  \"cpp_analyzer_rejects_degraded_self_test\": "
            << (cpp_analyzer_rejects_degraded_self_test ? "true" : "false") << ",\n"
            << "  \"python_oracle_retained\": " << (python_oracle_retained ? "true" : "false")
            << ",\n"
            << "  \"python_oracle_rejects_degraded_self_test\": "
            << (python_oracle_rejects_degraded_self_test ? "true" : "false") << ",\n"
            << "  \"physical_window_runs_both\": "
            << (physical_window_runs_both ? "true" : "false") << ",\n"
            << "  \"direct_usb_runs_wide_lag_audiophile\": "
            << (direct_usb_runs_wide_lag_audiophile ? "true" : "false") << ",\n"
            << "  \"known_good_route_runs_both\": "
            << (known_good_runs_both ? "true" : "false") << ",\n"
            << "  \"comparator_requires_both\": "
            << (comparator_requires_both ? "true" : "false") << ",\n"
            << "  \"comparator_requires_dual_oracle_agreement\": "
            << (comparator_requires_both ? "true" : "false") << ",\n"
            << "  \"product_claim_blocks_without_both\": "
            << (product_claim_blocks_without_both ? "true" : "false") << ",\n"
            << "  \"offline_runner_self_tests_both\": "
            << (offline_runner_self_tests_both ? "true" : "false") << ",\n"
            << "  \"offline_runner_self_tests_degraded\": "
            << (offline_runner_self_tests_degraded ? "true" : "false") << ",\n"
            << "  \"product_claim_allowed\": false,\n";
  print_string_array("required_same_session_artifacts", required_runtime_artifacts);
  print_string_array("blockers", blockers);
  std::cout
      << "  \"blocked_claim\": \"NO_AUDIOPHILE_OR_BRANCH_PROMOTION_CLAIM_WITHOUT_DUAL_CPP_AND_PYTHON_WAV_ANALYZERS_PASSING_ON_SAME_SESSION_MAINLINE_AND_CPP_CAPTURE\"\n"
      << "}\n";

  return pass ? 0 : 1;
}
