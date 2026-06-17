#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool file_contains(const std::filesystem::path& path, const std::string& needle) {
  std::ifstream input(path);
  if (!input) {
    return false;
  }
  const std::string data((std::istreambuf_iterator<char>(input)),
                         std::istreambuf_iterator<char>());
  return data.find(needle) != std::string::npos;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto executable = std::filesystem::absolute(argv[0]);
  std::filesystem::path root = executable.parent_path();
  while (!root.empty() && !std::filesystem::is_regular_file(root / "CMakeLists.txt")) {
    root = root.parent_path();
  }
  if (root.empty() || root.filename() != "audio8djcpp") {
    root = "/Users/fer/dev/audio8djcpp";
  }

  const std::vector<std::filesystem::path> required = {
      root / "docs/CANDIDATE_MANIFEST.json",
      root / "local-analysis/cpp-offline/current-offline-gates.json",
      root / "local-analysis/cpp-offline/ctest-default.txt",
      root / "local-analysis/cpp-offline/ctest-release.txt",
      root / "local-analysis/cpp-offline/packet-matrix.json",
      root / "local-analysis/cpp-offline/protocol-contract.json",
      root / "local-analysis/cpp-offline/simulated-output-matrix.json",
      root / "local-analysis/cpp-offline/mode2-python-oracle.txt",
      root / "local-analysis/cpp-offline/mode2-cross-oracle-parity.json",
      root / "local-analysis/cpp-offline/timecode-matrix.json",
      root / "local-analysis/cpp-offline/timecode-signal-analysis.json",
      root / "local-analysis/cpp-offline/timecode-readiness-gate.json",
      root / "local-analysis/cpp-offline/dvs-signal-smoke.json",
      root / "local-analysis/cpp-offline/dvs-packet-input-decode.json",
      root / "local-analysis/cpp-offline/realtime-audit.json",
      root / "local-analysis/cpp-offline/driverkit-surface-model.json",
      root / "local-analysis/cpp-offline/driverkit-shell-contract.json",
      root / "local-analysis/cpp-offline/driverkit-runtime-contract.json",
      root / "local-analysis/cpp-offline/driverkit-extension-scaffold-contract.json",
      root / "local-analysis/cpp-offline/driverkit-prepared-hotpath-contract.json",
      root / "local-analysis/cpp-offline/driverkit-usb-submit-binding-contract.json",
      root / "local-analysis/cpp-offline/driverkit-usb-request-lifecycle-contract.json",
      root / "local-analysis/cpp-offline/driverkit-usb-request-shutdown-contract.json",
      root / "local-analysis/cpp-offline/prepared-slot-scheduler-contract.json",
      root / "local-analysis/cpp-offline/runtime-adapter-contract.json",
      root / "local-analysis/cpp-offline/usb-submit-plan-contract.json",
      root / "local-analysis/cpp-offline/usb-submit-payload-contract.json",
      root / "local-analysis/cpp-offline/prepared-transport-pressure-gate.json",
      root / "local-analysis/cpp-offline/prepared-transport-migration-gate.json",
      root / "local-analysis/cpp-offline/jitter-model.json",
      root / "local-analysis/cpp-offline/loopback-quality-analysis.json",
      root / "local-analysis/cpp-offline/capture-matrix-quality-analysis.json",
      root / "local-analysis/cpp-offline/capture-route-health-gate.json",
      root / "local-analysis/cpp-offline/hot-path-timing-analysis.json",
      root / "local-analysis/cpp-offline/quality-root-cause-analysis.json",
      root / "local-analysis/cpp-offline/soundcheck-wav-quality.json",
      root / "local-analysis/cpp-offline/channel-leakage-tone-contract.json",
      root / "local-analysis/cpp-offline/audiophile-tone-gate.json",
      root / "local-analysis/cpp-offline/physical-run-product-superiority.json",
      root / "local-analysis/cpp-offline/physical-evidence-frontier.json",
      root / "local-analysis/cpp-offline/physical-capture-forensics.json",
      root / "local-analysis/cpp-offline/direct-usb-path-attribution.json",
      root / "local-analysis/cpp-offline/irig-idle-capture-gate.json",
      root / "local-analysis/cpp-offline/historical-route-reference-gate.json",
      root / "local-analysis/cpp-offline/hal-candidate-safety-gate.json",
      root / "local-analysis/cpp-offline/physical-window-readiness-gate.json",
      root / "local-analysis/cpp-offline/static-policy.json",
      root / "local-analysis/cpp-offline/hardware-lock-policy.json",
      root / "local-analysis/cpp-offline/promotion-readiness-offline-check.json",
      root / "local-analysis/cpp-offline/offline-bench-release.json",
      root / "docs/PHYSICAL_TEST_WINDOW_PLAN.md",
      root / "docs/OFFLINE_READINESS_REPORT.md",
  };

  std::uint32_t missing = 0;
  for (const auto& path : required) {
    if (!std::filesystem::is_regular_file(path)) {
      missing += 1;
    }
  }

  const bool summary_pass = file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"status\": \"PASS\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"diagnostic_status\": \"PASS\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"branch_promotion_allowed\": false") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"physical_measurement_valid_for_promotion\": false") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"promotion_hard_blockers\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "same_session_mainline_cpp_physical_ab_missing") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "traktor_timecode_vinyl_physical_gate_missing") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "runtime_cpu_superiority_over_mainline_missing") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "post_reboot_autologin_codex_resume_unfixed") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"runtime_adapter_contract\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"stable_usb_submit_reduction_ratio\": 8") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"usb_submit_plan_contract\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"stable_logical_slots\": 528") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"stable_total_frames\": 5808") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"usb_submit_plan_stable_usb_submit_calls\": 66") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"usb_submit_payload_contract\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"usb_submit_payload_descriptors\": 66") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"usb_submit_payload_total_frames\": 5808") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"driverkit_usb_submit_binding_contract\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"driverkit_usb_submit_binding_usb_submit_calls\": 66") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"driverkit_usb_submit_binding_total_frames\": 5808") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"driverkit_usb_request_lifecycle_contract\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"driverkit_usb_request_lifecycle_submit_calls\": 66") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"driverkit_usb_request_lifecycle_completed_frames\": 5808") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"driverkit_usb_request_shutdown_contract\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"driverkit_usb_request_shutdown_cancelled_requests\": 3") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"driverkit_usb_request_shutdown_live_requests_after_stop\": 0") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"physical_window_readiness_gate\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"ready_for_route_revalidation_window\": true") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"ready_for_product_physical_ab\": false") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"ready_for_branch_promotion\": false") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"ROUTE_REVALIDATION_ONLY\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"NO_PROMOTION_AB_UNTIL_ROUTE_PASS\"") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "NO_PRODUCT_AB_OR_BRANCH_PROMOTION_UNTIL_ROUTE_REVALIDATION_AND_SAME_SESSION_MAINLINE_CPP_PHYSICAL_COMPARE_PASS") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"hardware_touched\": false") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"coreaudio_touched\": false") &&
                            file_contains(root / "local-analysis/cpp-offline/current-offline-gates.json",
                                          "\"usb_touched\": false");

  const bool manifest_pass =
      file_contains(root / "docs/CANDIDATE_MANIFEST.json", "\"worktree\": \"/Users/fer/dev/audio8djcpp\"") &&
      file_contains(root / "docs/CANDIDATE_MANIFEST.json", "\"scope\": \"offline_only\"");

  const bool pass = missing == 0 && summary_pass && manifest_pass;
  std::cout << "{\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"required_files\": " << required.size() << ",\n"
            << "  \"missing_files\": " << missing << ",\n"
            << "  \"summary_pass\": " << (summary_pass ? "true" : "false") << ",\n"
            << "  \"manifest_pass\": " << (manifest_pass ? "true" : "false") << "\n"
            << "}\n";

  return pass ? 0 : 1;
}
