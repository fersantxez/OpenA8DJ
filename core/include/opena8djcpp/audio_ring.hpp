#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <span>

namespace opena8djcpp {

template <typename Frame, std::size_t Capacity>
class SpscFrameRing {
 public:
  static_assert(Capacity > 0);

  [[nodiscard]] constexpr std::size_t capacity() const {
    return Capacity;
  }

  [[nodiscard]] std::size_t readable() const {
    const auto read = read_index_.load(std::memory_order_acquire);
    const auto write = write_index_.load(std::memory_order_acquire);
    return distance(read, write);
  }

  [[nodiscard]] std::size_t writable() const {
    return Capacity - readable();
  }

  [[nodiscard]] bool push(const Frame& frame) {
    const auto write = write_index_.load(std::memory_order_relaxed);
    const auto next = increment(write);
    if (next == read_index_.load(std::memory_order_acquire)) {
      return false;
    }
    frames_[write] = frame;
    write_index_.store(next, std::memory_order_release);
    return true;
  }

  [[nodiscard]] bool pop(Frame& frame) {
    const auto read = read_index_.load(std::memory_order_relaxed);
    if (read == write_index_.load(std::memory_order_acquire)) {
      return false;
    }
    frame = frames_[read];
    read_index_.store(increment(read), std::memory_order_release);
    return true;
  }

  [[nodiscard]] std::size_t push_many(std::span<const Frame> frames) {
    std::size_t pushed = 0;
    for (const auto& frame : frames) {
      if (!push(frame)) {
        break;
      }
      pushed += 1;
    }
    return pushed;
  }

  [[nodiscard]] std::size_t pop_many(std::span<Frame> frames) {
    std::size_t popped = 0;
    for (auto& frame : frames) {
      if (!pop(frame)) {
        break;
      }
      popped += 1;
    }
    return popped;
  }

  void clear() {
    const auto write = write_index_.load(std::memory_order_acquire);
    read_index_.store(write, std::memory_order_release);
  }

 private:
  static constexpr std::size_t kStorageSize = Capacity + 1U;

  [[nodiscard]] static constexpr std::size_t increment(std::size_t index) {
    return (index + 1U) % kStorageSize;
  }

  [[nodiscard]] static constexpr std::size_t distance(std::size_t read, std::size_t write) {
    return write >= read ? write - read : (kStorageSize - read) + write;
  }

  std::array<Frame, kStorageSize> frames_{};
  std::atomic<std::size_t> read_index_{0};
  std::atomic<std::size_t> write_index_{0};
};

}  // namespace opena8djcpp
