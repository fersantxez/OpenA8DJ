#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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

bool contains_all(const std::string& data,
                  const std::vector<std::string>& needles,
                  std::vector<std::string>& missing,
                  const std::string& label) {
  bool ok = true;
  for (const auto& needle : needles) {
    if (data.find(needle) == std::string::npos) {
      ok = false;
      missing.push_back(label + ":" + needle);
    }
  }
  return ok;
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

  std::vector<std::string> missing;
  const auto lib = read_file(root / "scripts/hardware-lock-lib.sh");
  const auto safety = read_file(root / "scripts/test-hal-candidate-safety");
  const auto direct = read_file(root / "scripts/run-audio8dj-direct-gate");
  const auto direct_soundcheck = read_file(root / "scripts/run-direct-usb-soundcheck");
  const auto soundcheck = read_file(root / "scripts/run-soundcheck");
  const auto matrix = read_file(root / "scripts/run-channel-matrix-gate");

  const bool lib_ok = contains_all(lib,
                                   {
                                       "AUDIO_GATE_LOCK_ROOT",
                                       "opena8dj_acquire_hardware_lock()",
                                       "opena8dj_release_hardware_lock()",
                                       "audio_gate_lock=BUSY",
                                       "actions_authorized=",
                                       "forbidden=",
                                   },
                                   missing,
                                   "scripts/hardware-lock-lib.sh");

  const bool safety_ok = contains_all(safety,
                                      {
                                          "hardware-lock-lib.sh",
                                          "opena8dj_acquire_hardware_lock",
                                          "HAL install/reload",
                                          "coreaudiod restart",
                                          "trap opena8dj_release_hardware_lock EXIT",
                                      },
                                      missing,
                                      "scripts/test-hal-candidate-safety");

  const bool direct_ok = contains_all(direct,
                                      {
                                          "hardware-lock-lib.sh",
                                          "opena8dj_acquire_hardware_lock",
                                          "Audio 8 DJ direct CoreAudio I/O",
                                          "trap opena8dj_release_hardware_lock EXIT",
                                      },
                                      missing,
                                      "scripts/run-audio8dj-direct-gate");

  const bool direct_soundcheck_ok = contains_all(direct_soundcheck,
                                                 {
                                                     "hardware-lock-lib.sh",
                                                     "opena8dj_acquire_hardware_lock",
                                                     "physical-direct-usb-soundcheck",
                                                     "Audio 8 DJ direct USB playback, external capture",
                                                     "trap opena8dj_release_hardware_lock EXIT",
                                                     "--capture-device is required",
                                                 },
                                                 missing,
                                                 "scripts/run-direct-usb-soundcheck");

  const bool soundcheck_ok = contains_all(soundcheck,
                                          {
                                              "class HardwareLock",
                                              "AUDIO_GATE_LOCK_ROOT",
                                              "audio_gate_lock=BUSY",
                                              "physical-soundcheck",
                                              "Open Audio 8 DJ playback, external capture",
                                              "if not args.prepare_only",
                                              "with lock_context",
                                          },
                                          missing,
                                          "scripts/run-soundcheck");

  const bool matrix_ok = contains_all(matrix,
                                      {
                                          "hardware-lock-lib.sh",
                                          "opena8dj_acquire_hardware_lock",
                                          "physical-channel-matrix",
                                          "Open Audio 8 DJ playback, external capture",
                                          "no install, no unload, no USB reset, no service restart, no default-device change",
                                          "trap opena8dj_release_hardware_lock EXIT",
                                          "--run-physical",
                                          "--capture-device is required",
                                      },
                                      missing,
                                      "scripts/run-channel-matrix-gate");

  const bool pass = lib_ok && safety_ok && direct_ok && direct_soundcheck_ok &&
                    soundcheck_ok && matrix_ok;
  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.hardware-lock-policy.v1\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"audited_scripts\": 6,\n"
            << "  \"missing_requirements\": " << missing.size() << ",\n"
            << "  \"sensitive_paths\": [\n"
            << "    \"scripts/test-hal-candidate-safety\",\n"
            << "    \"scripts/run-audio8dj-direct-gate\",\n"
            << "    \"scripts/run-direct-usb-soundcheck\",\n"
            << "    \"scripts/run-soundcheck\",\n"
            << "    \"scripts/run-channel-matrix-gate\"\n"
            << "  ],\n"
            << "  \"missing\": [";
  for (std::size_t index = 0; index < missing.size(); ++index) {
    std::cout << (index == 0 ? "" : ", ") << "\"" << missing[index] << "\"";
  }
  std::cout << "]\n}\n";
  return pass ? 0 : 1;
}
