#include "evidence_json.hpp"

#include <iostream>
#include <string_view>

namespace {

bool expect(bool condition, std::string_view name) {
  if (!condition) {
    std::cerr << "FAIL " << name << "\n";
  }
  return condition;
}

}  // namespace

int main() {
  constexpr std::string_view sample = R"json(
{
  "schema": "opena8djcpp.evidence-json-contract.fixture",
  "branch_promotion_allowed": false,
  "measurement_valid_for_promotion": false,
  "product_candidate_runs": 0,
  "latest_run": {"internal_clean": true, "capture_failed": true},
  "promotion_blockers": ["shared_capture_route_unhealthy", "same_session_mainline_cpp_physical_ab_on_validated_route"],
  "gates": [
    {"name": "runtime_cpu_beats_mainline", "result": "FAIL"},
    {"name": "traktor_timecode_physical", "result": "FAIL"}
  ],
  "sensitive_paths": ["scripts/run-known-good-route-soundcheck"],
  "result": "PASS"
}
)json";

  const auto latest_run = opena8djcpp::evidence_json::json_object(sample, "latest_run");
  const bool pass =
      expect(opena8djcpp::evidence_json::json_string(sample, "schema").value_or("") ==
                 "opena8djcpp.evidence-json-contract.fixture",
             "string field") &&
      expect(opena8djcpp::evidence_json::json_bool(sample, "branch_promotion_allowed").value_or(true) ==
                 false,
             "bool false field") &&
      expect(opena8djcpp::evidence_json::json_number(sample, "product_candidate_runs").value_or(-1.0) ==
                 0.0,
             "number field") &&
      expect(latest_run.has_value(), "nested object present") &&
      expect(opena8djcpp::evidence_json::json_bool(*latest_run, "internal_clean").value_or(false),
             "nested bool true") &&
      expect(opena8djcpp::evidence_json::json_string_array_contains(
                 sample, "promotion_blockers", "shared_capture_route_unhealthy"),
             "string array value") &&
      expect(opena8djcpp::evidence_json::json_object_array_contains_string_field(
                 sample, "gates", "name", "traktor_timecode_physical"),
             "object array string field") &&
      expect(opena8djcpp::evidence_json::json_string_last(sample, "result").value_or("") == "PASS",
             "last string field") &&
      expect(opena8djcpp::evidence_json::json_string_array_contains(
                 sample, "sensitive_paths", "scripts/run-known-good-route-soundcheck"),
             "path array value");

  std::cout << "{\n"
            << "  \"schema\": \"opena8djcpp.evidence-json-contract.v1\",\n"
            << "  \"result\": \"" << (pass ? "PASS" : "FAIL") << "\"\n"
            << "}\n";
  return pass ? 0 : 1;
}
