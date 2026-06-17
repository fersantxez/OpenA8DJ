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
#include <string>
#include <vector>

namespace {

struct HotPathEvidence {
  std::filesystem::path path;
  double capture_transfers_per_second = std::numeric_limits<double>::quiet_NaN();
  double playback_transfers_per_second = std::numeric_limits<double>::quiet_NaN();
  double capture_handler = std::numeric_limits<double>::quiet_NaN();
  double capture_decode = std::numeric_limits<double>::quiet_NaN();
  double capture_requeue = std::numeric_limits<double>::quiet_NaN();
  double playback_queue = std::numeric_limits<double>::quiet_NaN();
  double playback_fill = std::numeric_limits<double>::quiet_NaN();
  double playback_enqueue = std::numeric_limits<double>::quiet_NaN();
  double playback_completion = std::numeric_limits<double>::quiet_NaN();
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
  try {
    const double value = std::stod(json.substr(start, end - start));
    return std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

double number_or_nan(std::optional<double> value) {
  return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

bool finite_positive(double value) {
  return std::isfinite(value) && value > 0.0;
}

HotPathEvidence read_evidence(const std::filesystem::path& path) {
  const auto json = read_file(path);
  HotPathEvidence evidence{};
  evidence.path = path;
  evidence.capture_transfers_per_second =
      number_or_nan(json_number(json, "capture_transfers_per_second"));
  evidence.playback_transfers_per_second =
      number_or_nan(json_number(json, "playback_transfers_completed_per_second"));
  evidence.capture_handler = number_or_nan(json_number(json, "capture_handler"));
  evidence.capture_decode = number_or_nan(json_number(json, "capture_decode"));
  evidence.capture_requeue = number_or_nan(json_number(json, "capture_requeue"));
  evidence.playback_queue = number_or_nan(json_number(json, "playback_queue"));
  evidence.playback_fill = number_or_nan(json_number(json, "playback_fill"));
  evidence.playback_enqueue = number_or_nan(json_number(json, "playback_enqueue"));
  evidence.playback_completion = number_or_nan(json_number(json, "playback_completion"));
  return evidence;
}

bool has_hot_path_ticks(const HotPathEvidence& evidence) {
  return finite_positive(evidence.capture_handler) && finite_positive(evidence.capture_requeue) &&
         finite_positive(evidence.playback_queue) && finite_positive(evidence.playback_enqueue) &&
         finite_positive(evidence.playback_fill);
}

std::vector<HotPathEvidence> load_hot_path_runs(const std::filesystem::path& root) {
  std::vector<HotPathEvidence> runs;
  const auto base = root / "local-analysis/hot-path-timing";
  if (!std::filesystem::is_directory(base)) {
    return runs;
  }
  for (const auto& entry : std::filesystem::directory_iterator(base)) {
    if (!entry.is_directory()) {
      continue;
    }
    const auto summary = entry.path() / "stream-stats-summary.json";
    if (!std::filesystem::is_regular_file(summary)) {
      continue;
    }
    auto evidence = read_evidence(summary);
    if (has_hot_path_ticks(evidence)) {
      runs.push_back(evidence);
    }
  }
  std::sort(runs.begin(), runs.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.path.string() < rhs.path.string();
  });
  return runs;
}

void print_json_string(const std::string& value) {
  std::cout << '"';
  for (const char c : value) {
    if (c == '"' || c == '\\') {
      std::cout << '\\';
    }
    if (c == '\n') {
      std::cout << "\\n";
    } else {
      std::cout << c;
    }
  }
  std::cout << '"';
}

void print_json_number(double value) {
  if (std::isfinite(value)) {
    std::cout << value;
  } else {
    std::cout << "null";
  }
}

std::string dominant_subsegment(const HotPathEvidence& evidence) {
  struct Segment {
    const char* name;
    double value;
  };
  std::vector<Segment> segments = {
      {"capture_requeue", evidence.capture_requeue},
      {"playback_queue", evidence.playback_queue},
      {"playback_enqueue", evidence.playback_enqueue},
      {"playback_fill", evidence.playback_fill},
      {"playback_completion", evidence.playback_completion},
      {"capture_decode", evidence.capture_decode},
  };
  const auto best = std::max_element(segments.begin(), segments.end(), [](const auto& a, const auto& b) {
    return number_or_nan(a.value) < number_or_nan(b.value);
  });
  return best == segments.end() ? "unknown" : best->name;
}

std::string attribution(const HotPathEvidence& evidence) {
  const double fixed_queue =
      evidence.capture_requeue + evidence.playback_queue + evidence.playback_enqueue;
  if (std::isfinite(fixed_queue) && std::isfinite(evidence.playback_fill) &&
      fixed_queue > evidence.playback_fill * 6.0) {
    return "fixed_queue_requeue_enqueue_dominant";
  }
  if (std::isfinite(evidence.playback_fill) && std::isfinite(fixed_queue) &&
      evidence.playback_fill > fixed_queue) {
    return "playback_fill_dominant";
  }
  return "mixed_or_low_confidence";
}

}  // namespace

int main(int argc, char** argv) {
  const auto root = repo_root(argv);
  std::vector<HotPathEvidence> runs;
  if (argc > 1) {
    for (int index = 1; index < argc; ++index) {
      auto evidence = read_evidence(argv[index]);
      if (has_hot_path_ticks(evidence)) {
        runs.push_back(evidence);
      }
    }
  } else {
    runs = load_hot_path_runs(root);
  }

  std::cout << std::fixed << std::setprecision(6);
  std::cout << "{\n";
  std::cout << "  \"schema\": \"opena8djcpp.hot-path-timing-analysis.v1\",\n";
  std::cout << "  \"safety\": \"offline_existing_stream_stats_only_no_audio_coreaudio_usb_or_hardware_touch\",\n";
  if (runs.empty()) {
    std::cout << "  \"result\": \"PASS\",\n"
              << "  \"mode\": \"no_existing_hot_path_timing_evidence\",\n"
              << "  \"meaning\": \"analyzer compiled but no stored nonzero hot-path timing evidence was available\",\n"
              << "  \"readiness_claim\": \"NO_CPU_ATTRIBUTION_EVIDENCE\"\n"
              << "}\n";
    return 0;
  }

  const auto& selected = runs.back();
  const double fixed_queue =
      selected.capture_requeue + selected.playback_queue + selected.playback_enqueue;
  const double nested_sum = selected.capture_decode + selected.capture_requeue +
                            selected.playback_queue + selected.playback_fill +
                            selected.playback_enqueue + selected.playback_completion;
  const double nested_to_handler =
      finite_positive(selected.capture_handler) ? nested_sum / selected.capture_handler
                                                : std::numeric_limits<double>::quiet_NaN();
  const double fixed_to_fill =
      finite_positive(selected.playback_fill) ? fixed_queue / selected.playback_fill
                                              : std::numeric_limits<double>::quiet_NaN();
  const double capture_handler_ticks_per_second =
      selected.capture_handler * selected.capture_transfers_per_second;
  const double fixed_queue_ticks_per_second = fixed_queue * selected.capture_transfers_per_second;

  std::cout << "  \"result\": \"PASS\",\n";
  std::cout << "  \"mode\": \"stored_hot_path_timing_evidence\",\n";
  std::cout << "  \"selected_run\": ";
  print_json_string(selected.path.string());
  std::cout << ",\n";
  std::cout << "  \"run_count\": " << runs.size() << ",\n";
  std::cout << "  \"capture_transfers_per_second\": ";
  print_json_number(selected.capture_transfers_per_second);
  std::cout << ",\n";
  std::cout << "  \"playback_transfers_per_second\": ";
  print_json_number(selected.playback_transfers_per_second);
  std::cout << ",\n";
  std::cout << "  \"hot_path_average_ticks\": {\n"
            << "    \"capture_handler\": " << selected.capture_handler << ",\n"
            << "    \"capture_decode\": " << selected.capture_decode << ",\n"
            << "    \"capture_requeue\": " << selected.capture_requeue << ",\n"
            << "    \"playback_queue\": " << selected.playback_queue << ",\n"
            << "    \"playback_fill\": " << selected.playback_fill << ",\n"
            << "    \"playback_enqueue\": " << selected.playback_enqueue << ",\n"
            << "    \"playback_completion\": " << selected.playback_completion << "\n"
            << "  },\n";
  std::cout << "  \"dominant_subsegment\": ";
  print_json_string(dominant_subsegment(selected));
  std::cout << ",\n";
  std::cout << "  \"attribution\": ";
  print_json_string(attribution(selected));
  std::cout << ",\n";
  std::cout << "  \"fixed_queue_requeue_enqueue_ticks\": ";
  print_json_number(fixed_queue);
  std::cout << ",\n";
  std::cout << "  \"fixed_queue_to_playback_fill_ratio\": ";
  print_json_number(fixed_to_fill);
  std::cout << ",\n";
  std::cout << "  \"nested_sum_to_capture_handler_ratio\": ";
  print_json_number(nested_to_handler);
  std::cout << ",\n";
  std::cout << "  \"nested_timing_policy\": ";
  print_json_string(nested_to_handler > 1.05 ? "nested_or_sampled_do_not_sum_as_total_cpu"
                                             : "nested_sum_within_handler");
  std::cout << ",\n";
  std::cout << "  \"capture_handler_ticks_per_second\": ";
  print_json_number(capture_handler_ticks_per_second);
  std::cout << ",\n";
  std::cout << "  \"fixed_queue_ticks_per_second\": ";
  print_json_number(fixed_queue_ticks_per_second);
  std::cout << ",\n";
  std::cout << "  \"readiness_claim\": \"DIAGNOSTIC_ONLY_NOT_PRODUCT_READINESS\"\n";
  std::cout << "}\n";
  return 0;
}
