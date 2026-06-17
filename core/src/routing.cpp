#include "opena8djcpp/routing.hpp"

#include <algorithm>

namespace opena8djcpp {

RoutingPlan::RoutingPlan(const RoutingMatrix& routing)
    : mapping_(routing.mapping()), routes_(routing.routes()) {
  valid_ = true;
  identity_ = true;
  for (std::uint32_t output_channel = 0; output_channel < kOutputChannels; ++output_channel) {
    const auto route = routes_[output_channel];
    if (!route.is_muted() && route.source_channel >= kInputChannels) {
      valid_ = false;
    }
    if (route.source_channel != output_channel || route.gain != 1) {
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

  if (plan.is_identity()) {
    std::copy_n(input.begin(), required, output.begin());
    return true;
  }

  const auto& routes = plan.routes();
  for (std::uint32_t frame = 0; frame < frames; ++frame) {
    const auto base = static_cast<std::size_t>(frame) * kOutputChannels;
    for (std::uint32_t output_channel = 0; output_channel < kOutputChannels; ++output_channel) {
      const auto route = routes[output_channel];
      output[base + output_channel] =
          input[base + route.source_channel] * static_cast<float>(route.gain);
    }
  }

  return true;
}

bool route_s24_frames(std::span<const S24Frame> input,
                      std::span<S24Frame> output,
                      const RoutingMatrix& routing) {
  const RoutingPlan plan(routing);
  return route_s24_frames(input, output, plan);
}

bool route_s24_frames(std::span<const S24Frame> input,
                      std::span<S24Frame> output,
                      const RoutingPlan& plan) {
  if (!plan.valid() || output.size() < input.size()) {
    return false;
  }

  if (plan.is_identity()) {
    std::copy(input.begin(), input.end(), output.begin());
    return true;
  }

  const auto& routes = plan.routes();
  for (std::size_t frame = 0; frame < input.size(); ++frame) {
    for (std::uint32_t output_channel = 0; output_channel < kOutputChannels; ++output_channel) {
      const auto route = routes[output_channel];
      output[frame][output_channel] =
          route.is_muted()
              ? 0
              : input[frame][route.source_channel] * static_cast<std::int32_t>(route.gain);
    }
  }
  return true;
}

}  // namespace opena8djcpp
