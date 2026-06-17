#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace opena8djcpp::evidence_json {

inline void skip_ws(std::string_view text, std::size_t& index) {
  while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index]))) {
    ++index;
  }
}

inline std::optional<std::size_t> find_value_start(std::string_view json,
                                                   std::string_view key,
                                                   std::size_t from = 0U) {
  const std::string needle = "\"" + std::string(key) + "\"";
  const std::size_t key_pos = json.find(needle, from);
  if (key_pos == std::string_view::npos) {
    return std::nullopt;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string_view::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  skip_ws(json, start);
  return start < json.size() ? std::optional<std::size_t>(start) : std::nullopt;
}

inline std::optional<std::size_t> find_last_value_start(std::string_view json,
                                                        std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  const std::size_t key_pos = json.rfind(needle);
  if (key_pos == std::string_view::npos) {
    return std::nullopt;
  }
  const std::size_t colon = json.find(':', key_pos + needle.size());
  if (colon == std::string_view::npos) {
    return std::nullopt;
  }
  std::size_t start = colon + 1U;
  skip_ws(json, start);
  return start < json.size() ? std::optional<std::size_t>(start) : std::nullopt;
}

inline std::optional<std::string> parse_string_at(std::string_view json, std::size_t start) {
  if (start >= json.size() || json[start] != '"') {
    return std::nullopt;
  }
  std::string value;
  bool escaped = false;
  for (std::size_t index = start + 1U; index < json.size(); ++index) {
    const char c = json[index];
    if (escaped) {
      switch (c) {
        case '"':
        case '\\':
        case '/':
          value.push_back(c);
          break;
        case 'b':
          value.push_back('\b');
          break;
        case 'f':
          value.push_back('\f');
          break;
        case 'n':
          value.push_back('\n');
          break;
        case 'r':
          value.push_back('\r');
          break;
        case 't':
          value.push_back('\t');
          break;
        default:
          value.push_back(c);
          break;
      }
      escaped = false;
      continue;
    }
    if (c == '\\') {
      escaped = true;
      continue;
    }
    if (c == '"') {
      return value;
    }
    value.push_back(c);
  }
  return std::nullopt;
}

inline std::optional<std::string> json_string(std::string_view json, std::string_view key) {
  const auto start = find_value_start(json, key);
  if (!start) {
    return std::nullopt;
  }
  return parse_string_at(json, *start);
}

inline std::optional<std::string> json_string_last(std::string_view json, std::string_view key) {
  const auto start = find_last_value_start(json, key);
  if (!start) {
    return std::nullopt;
  }
  return parse_string_at(json, *start);
}

inline std::optional<bool> json_bool(std::string_view json, std::string_view key) {
  const auto start = find_value_start(json, key);
  if (!start) {
    return std::nullopt;
  }
  if (json.substr(*start, 4U) == "true") {
    return true;
  }
  if (json.substr(*start, 5U) == "false") {
    return false;
  }
  return std::nullopt;
}

inline std::optional<double> json_number(std::string_view json, std::string_view key) {
  const auto start = find_value_start(json, key);
  if (!start) {
    return std::nullopt;
  }
  std::size_t end = *start;
  while (end < json.size()) {
    const char c = json[end];
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.' ||
          c == 'e' || c == 'E')) {
      break;
    }
    ++end;
  }
  if (end == *start) {
    return std::nullopt;
  }
  try {
    const double value = std::stod(std::string(json.substr(*start, end - *start)));
    return std::isfinite(value) ? std::optional<double>(value) : std::nullopt;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

inline std::optional<std::string_view> balanced_value(std::string_view json,
                                                      std::string_view key,
                                                      char open,
                                                      char close) {
  const auto start = find_value_start(json, key);
  if (!start || json[*start] != open) {
    return std::nullopt;
  }
  std::uint32_t object_depth = 0;
  std::uint32_t array_depth = 0;
  bool in_string = false;
  bool escaped = false;
  for (std::size_t index = *start; index < json.size(); ++index) {
    const char c = json[index];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (in_string && c == '\\') {
      escaped = true;
      continue;
    }
    if (c == '"') {
      in_string = !in_string;
      continue;
    }
    if (in_string) {
      continue;
    }
    if (c == '{') {
      ++object_depth;
    } else if (c == '}') {
      if (object_depth == 0U) {
        return std::nullopt;
      }
      --object_depth;
    } else if (c == '[') {
      ++array_depth;
    } else if (c == ']') {
      if (array_depth == 0U) {
        return std::nullopt;
      }
      --array_depth;
    }
    if (c == close && object_depth == 0U && array_depth == 0U) {
        return json.substr(*start, index - *start + 1U);
    }
  }
  return std::nullopt;
}

inline std::optional<std::string_view> json_object(std::string_view json, std::string_view key) {
  return balanced_value(json, key, '{', '}');
}

inline std::optional<std::string_view> json_array(std::string_view json, std::string_view key) {
  return balanced_value(json, key, '[', ']');
}

inline bool json_string_array_contains(std::string_view json,
                                       std::string_view key,
                                       std::string_view expected) {
  const auto array = json_array(json, key);
  if (!array) {
    return false;
  }
  std::size_t index = 1U;
  while (index + 1U < array->size()) {
    skip_ws(*array, index);
    if ((*array)[index] == ',') {
      ++index;
      continue;
    }
    if ((*array)[index] == '"') {
      const auto value = parse_string_at(*array, index);
      if (value && *value == expected) {
        return true;
      }
      ++index;
      while (index < array->size() && ((*array)[index] != '"' || (*array)[index - 1U] == '\\')) {
        ++index;
      }
      if (index < array->size()) {
        ++index;
      }
      continue;
    }
    ++index;
  }
  return false;
}

inline bool json_object_array_contains_string_field(std::string_view json,
                                                    std::string_view array_key,
                                                    std::string_view field_key,
                                                    std::string_view expected) {
  const auto array = json_array(json, array_key);
  if (!array) {
    return false;
  }
  std::size_t search_from = 0U;
  while (true) {
    const auto start = find_value_start(*array, field_key, search_from);
    if (!start) {
      return false;
    }
    const auto value = parse_string_at(*array, *start);
    if (value && *value == expected) {
      return true;
    }
    search_from = *start + 1U;
  }
}

}  // namespace opena8djcpp::evidence_json
