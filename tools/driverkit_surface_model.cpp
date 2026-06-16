#include "opena8djcpp/driverkit_model.hpp"

#include <cstdint>
#include <iostream>

using namespace opena8djcpp;

int main() {
  const auto model = make_driverkit_device_model();
  const bool ok = validate_driverkit_device_model(model);

  std::cout << "{\n"
            << "  \"result\": \"" << (ok ? "PASS" : "FAIL") << "\",\n"
            << "  \"device_name\": \"" << model.device_name << "\",\n"
            << "  \"uid\": \"" << model.uid << "\",\n"
            << "  \"sample_rates\": [" << model.sample_rates[0] << ", "
            << model.sample_rates[1] << "],\n"
            << "  \"streams\": [\n";

  for (std::uint32_t index = 0; index < model.streams.size(); ++index) {
    const auto& stream = model.streams[index];
    std::cout << "    {\"name\": \"" << stream.name << "\", \"direction\": \""
              << (stream.direction == StreamDirection::Input ? "input" : "output")
              << "\", \"starting_channel\": " << stream.starting_channel
              << ", \"channel_count\": " << stream.channel_count << "}"
              << (index + 1 == model.streams.size() ? "\n" : ",\n");
  }
  std::cout << "  ]\n}\n";

  return ok ? 0 : 1;
}
