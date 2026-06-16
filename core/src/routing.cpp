#include "opena8djcpp/routing.hpp"

#include <algorithm>

namespace opena8djcpp {

RoutingPlan::RoutingPlan(const RoutingMatrix& routing) : mapping_(routing.mapping()) {
  valid_ = true;
  identity_ = true;
  for (std::uint32_t output_channel = 0; output_channel < kOutputChannels; ++output_channel) {
    const auto input_channel = mapping_[output_channel];
    if (input_channel >= kInputChannels) {
      valid_ = false;
    }
    if (input_channel != output_channel) {
      identity_ = false;
    }
  }
}

bool route_interleaved_f32(std::span<const float> input,
                           std::span<float> output,
                           std::uint32_t frames,
                           const RoutingMatrix& routing) {
  const RoutingPlan plan(routing);
  return route_interleaved_f32(input, output, frames, plan);
}

bool route_interleaved_f32(std::span<const float> input,
                           std::span<float> output,
                           std::uint32_t frames,
                           const RoutingPlan& plan) {
  const auto required = static_cast<std::size_t>(frames) * kOutputChannels;
  if (!plan.valid() || input.size() < required || output.size() < required) {
    return false;
  }

  const auto& mapping = plan.mapping();
  if (plan.is_identity()) {
    std::copy_n(input.begin(), required, output.begin());
    return true;
  }

  for (std::uint32_t frame = 0; frame < frames; ++frame) {
    const auto base = static_cast<std::size_t>(frame) * kOutputChannels;
    for (std::uint32_t output_channel = 0; output_channel < kOutputChannels; ++output_channel) {
      const auto input_channel = mapping[output_channel];
      output[base + output_channel] = input[base + input_channel];
    }
  }

  return true;
}

}  // namespace opena8djcpp
