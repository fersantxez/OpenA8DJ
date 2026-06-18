#include "evidence_json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path);
  if (!input) {
    return {};
  }
  return std::string((std::istreambuf_iterator<char>(input)),
                     std::istreambuf_iterator<char>());
}

std::string trim(std::string value) {
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), is_space));
  value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(), value.end());
  return value;
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

std::optional<std::filesystem::path> git_dir(const std::filesystem::path& root) {
  const auto dot_git = root / ".git";
  if (std::filesystem::is_directory(dot_git)) {
    return dot_git;
  }
  const auto text = trim(read_file(dot_git));
  constexpr std::string_view prefix = "gitdir:";
  if (text.rfind(prefix, 0) != 0) {
    return std::nullopt;
  }
  auto path = trim(text.substr(prefix.size()));
  std::filesystem::path parsed(path);
  if (parsed.is_relative()) {
    parsed = root / parsed;
  }
  return std::filesystem::weakly_canonical(parsed);
}

std::filesystem::path common_git_dir(const std::filesystem::path& git) {
  const auto common = trim(read_file(git / "commondir"));
  if (common.empty()) {
    return git;
  }
  std::filesystem::path path(common);
  if (path.is_relative()) {
    path = git / path;
  }
  return std::filesystem::weakly_canonical(path);
}

std::optional<std::string> packed_ref(const std::filesystem::path& common,
                                      std::string_view ref_name) {
  const auto packed = read_file(common / "packed-refs");
  std::size_t start = 0;
  while (start < packed.size()) {
    const auto end = packed.find('\n', start);
    const auto line = packed.substr(start, end == std::string::npos ? std::string::npos : end - start);
    start = end == std::string::npos ? packed.size() : end + 1U;
    if (line.empty() || line[0] == '#' || line[0] == '^') {
      continue;
    }
    const auto split = line.find(' ');
    if (split == std::string::npos) {
      continue;
    }
    if (line.substr(split + 1U) == ref_name) {
      return line.substr(0, split);
    }
  }
  return std::nullopt;
}

std::optional<std::string> head_commit(const std::filesystem::path& root) {
  const auto maybe_git = git_dir(root);
  if (!maybe_git) {
    return std::nullopt;
  }
  const auto git = *maybe_git;
  const auto common = common_git_dir(git);
  const auto head = trim(read_file(git / "HEAD"));
  constexpr std::string_view ref_prefix = "ref:";
  if (head.rfind(ref_prefix, 0) != 0) {
    return head.empty() ? std::nullopt : std::optional<std::string>{head};
  }
  const auto ref_name = trim(head.substr(ref_prefix.size()));
  for (const auto& base : {git, common}) {
    const auto ref_text = trim(read_file(base / ref_name));
    if (!ref_text.empty()) {
      return ref_text;
    }
  }
  return packed_ref(common, ref_name);
}

std::string short_commit(std::string value) {
  value = trim(std::move(value));
  return value.size() > 7U ? value.substr(0, 7U) : value;
}

bool string_field_is(std::string_view json, std::string_view key, std::string_view expected) {
  return opena8djcpp::evidence_json::json_string(json, key).value_or("") == expected;
}

bool bool_field_is(std::string_view json, std::string_view key, bool expected) {
  return opena8djcpp::evidence_json::json_bool(json, key).value_or(!expected) == expected;
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  const auto root = repo_root(argv);
  const auto summary = read_file(root / "local-analysis/cpp-offline/current-offline-gates.json");
  const auto manifest = read_file(root / "docs/CANDIDATE_MANIFEST.json");
  const auto actual = head_commit(root);
  const auto actual_short = actual ? short_commit(*actual) : std::string{};
  const auto summary_commit =
      opena8djcpp::evidence_json::json_string(summary, "base_commit").value_or("");
  const auto manifest_commit =
      opena8djcpp::evidence_json::json_string(manifest, "base_commit").value_or("");

  const bool evidence_present = !summary.empty() && !manifest.empty() && actual.has_value();
  const bool summary_matches_head = evidence_present && summary_commit == actual_short;
  const bool manifest_declares_base = !manifest_commit.empty();
  const bool worktree_clean_for_claim = bool_field_is(summary, "working_tree_dirty", false);
  const bool no_touch = bool_field_is(summary, "hardware_touched", false) &&
                        bool_field_is(summary, "coreaudio_touched", false) &&
                        bool_field_is(summary, "usb_touched", false);
  const bool offline_pass = string_field_is(summary, "status", "PASS");
  const bool pass =
      evidence_present && summary_matches_head && worktree_clean_for_claim && no_touch && offline_pass;

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.evidence-provenance-freshness-gate.v1\",\n"
            << "  \"safety\": \"offline_git_metadata_and_existing_evidence_only_no_audio_coreaudio_usb_or_hardware_touch\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\",\n"
            << "  \"meaning\": \"PASS means current offline evidence is attributable to current HEAD and a clean claimable worktree\",\n"
            << "  \"evidence_present\": " << (evidence_present ? "true" : "false") << ",\n"
            << "  \"head_commit\": \"" << actual_short << "\",\n"
            << "  \"summary_base_commit\": \"" << summary_commit << "\",\n"
            << "  \"summary_matches_head\": " << (summary_matches_head ? "true" : "false") << ",\n"
            << "  \"manifest_base_commit\": \"" << manifest_commit << "\",\n"
            << "  \"manifest_declares_base\": " << (manifest_declares_base ? "true" : "false")
            << ",\n"
            << "  \"working_tree_clean_for_claim\": "
            << (worktree_clean_for_claim ? "true" : "false") << ",\n"
            << "  \"offline_summary_pass\": " << (offline_pass ? "true" : "false") << ",\n"
            << "  \"no_hardware_coreaudio_usb_touch\": " << (no_touch ? "true" : "false")
            << ",\n"
            << "  \"claimable_current_candidate\": " << (pass ? "true" : "false") << ",\n"
            << "  \"blocked_claim\": \"NO_CURRENT_CANDIDATE_CLAIM_IF_EVIDENCE_COMMIT_OR_WORKTREE_IS_STALE\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
