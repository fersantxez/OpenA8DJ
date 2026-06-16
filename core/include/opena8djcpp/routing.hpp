#pragma once

#include "opena8djcpp/audio_model.hpp"

#include <array>
#include <span>

namespace opena8djcpp {

class RoutingMatrix {
 public:
  using Mapping = std::array<std::uint32_t, kOutputChannels>;

  constexpr explicit RoutingMatrix(Mapping output_to_input) : output_to_input_(output_to_input) {}

  [[nodiscard]] static constexpr RoutingMatrix identity() {
    return RoutingMatrix({0, 1, 2, 3, 4, 5, 6, 7});
  }

  [[nodiscard]] constexpr std::uint32_t source_for_output(std::uint32_t output_channel) const {
    return output_to_input_[output_channel];
  }

  [[nodiscard]] constexpr const Mapping& mapping() const {
    return output_to_input_;
  }

  [[nodiscard]] constexpr bool is_identity() const {
    for (std::uint32_t channel = 0; channel < kOutputChannels; ++channel) {
      if (output_to_input_[channel] != channel) {
        return false;
      }
    }
    return true;
  }

 private:
  Mapping output_to_input_;
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

 private:
  RoutingMatrix::Mapping mapping_{};
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

}  // namespace opena8djcpp
