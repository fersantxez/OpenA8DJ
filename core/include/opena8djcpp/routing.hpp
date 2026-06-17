#pragma once

#include "opena8djcpp/audio_model.hpp"
#include "opena8djcpp/mode2_packet.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace opena8djcpp {

struct RouteEntry {
  std::uint32_t source_channel = 0;
  std::int8_t gain = 0;

  [[nodiscard]] static constexpr RouteEntry passthrough(std::uint32_t source) {
    return RouteEntry{source, 1};
  }

  [[nodiscard]] static constexpr RouteEntry inverted(std::uint32_t source) {
    return RouteEntry{source, -1};
  }

  [[nodiscard]] static constexpr RouteEntry muted() {
    return RouteEntry{0, 0};
  }

  [[nodiscard]] constexpr bool is_muted() const {
    return gain == 0;
  }

  [[nodiscard]] constexpr bool is_inverted() const {
    return gain < 0;
  }
};

class RoutingMatrix {
 public:
  using Mapping = std::array<std::uint32_t, kOutputChannels>;
  using Routes = std::array<RouteEntry, kOutputChannels>;

  constexpr explicit RoutingMatrix(Mapping output_to_input) : mapping_(output_to_input) {
    for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
      routes_[channel] = RouteEntry::passthrough(output_to_input[channel]);
    }
  }

  constexpr explicit RoutingMatrix(Routes routes) : routes_(routes) {
    for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
      mapping_[channel] = routes[channel].source_channel;
    }
  }

  [[nodiscard]] static constexpr RoutingMatrix identity() {
    return RoutingMatrix(Mapping{0, 1, 2, 3, 4, 5, 6, 7});
  }

  [[nodiscard]] static constexpr RoutingMatrix muted() {
    return RoutingMatrix(Routes{RouteEntry::muted(),
                                RouteEntry::muted(),
                                RouteEntry::muted(),
                                RouteEntry::muted(),
                                RouteEntry::muted(),
                                RouteEntry::muted(),
                                RouteEntry::muted(),
                                RouteEntry::muted()});
  }

  [[nodiscard]] static constexpr RoutingMatrix dvs_default() {
    return identity();
  }

  [[nodiscard]] constexpr std::uint32_t source_for_output(std::uint32_t output_channel) const {
    return mapping_[output_channel];
  }

  [[nodiscard]] constexpr RouteEntry route_for_output(std::uint32_t output_channel) const {
    return routes_[output_channel];
  }

  [[nodiscard]] constexpr const Mapping& mapping() const {
    return mapping_;
  }

  [[nodiscard]] constexpr const Routes& routes() const {
    return routes_;
  }

  [[nodiscard]] constexpr bool is_identity() const {
    for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
      if (routes_[channel].source_channel != channel || routes_[channel].gain != 1) {
        return false;
      }
    }
    return true;
  }

 private:
  Mapping mapping_{};
  Routes routes_{};
};

class RoutingPlan {
 public:
  explicit RoutingPlan(const RoutingMatrix& routing);

  [[nodiscard]] bool valid() const {
    return valid_;
  }

  [[nodiscard]] bool is_identity() const {
    return identity_;
  }

  [[nodiscard]] const RoutingMatrix::Mapping& mapping() const {
    return mapping_;
  }

  [[nodiscard]] const RoutingMatrix::Routes& routes() const {
    return routes_;
  }

 private:
  RoutingMatrix::Mapping mapping_{};
  RoutingMatrix::Routes routes_{};
  bool valid_ = false;
  bool identity_ = false;
};

bool route_interleaved_f32(std::span<const float> input,
                           std::span<float> output,
                           std::uint32_t frames,
                           const RoutingMatrix& routing);

bool route_interleaved_f32(std::span<const float> input,
                           std::span<float> output,
                           std::uint32_t frames,
                           const RoutingPlan& plan);

bool route_s24_frames(std::span<const S24Frame> input,
                      std::span<S24Frame> output,
                      const RoutingPlan& plan);

bool route_s24_frames(std::span<const S24Frame> input,
                      std::span<S24Frame> output,
                      const RoutingMatrix& routing);

}  // namespace opena8djcpp
