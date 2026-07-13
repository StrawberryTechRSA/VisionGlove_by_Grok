#pragma once
// Single-producer single-consumer lock-free ring buffer.
// Claude's Python version used asyncio + unbounded lists — unbounded latency under load.
// This gives fixed capacity, wait-free push/pop for the hot path.

#include <atomic>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace vg {

template <typename T, std::size_t Capacity>
class SpscRingBuffer {
    static_assert(Capacity >= 2, "Capacity must be >= 2");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
    static_assert(std::is_nothrow_move_constructible_v<T> || std::is_copy_constructible_v<T>);

public:
    SpscRingBuffer() = default;
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    // Producer thread only
    [[nodiscard]] bool try_push(T item) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next = (head + 1) & mask_;
        if (next == tail_.load(std::memory_order_acquire)) {
            return false;  // full — drop or backpressure policy is caller's choice
        }
        slots_[head] = std::move(item);
        head_.store(next, std::memory_order_release);
        return true;
    }

    // Consumer thread only
    [[nodiscard]] std::optional<T> try_pop() {
        const auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        T item = std::move(slots_[tail]);
        tail_.store((tail + 1) & mask_, std::memory_order_release);
        return item;
    }

    // Consumer: drain to latest (keep only newest) — ideal for sensor/vision
    [[nodiscard]] std::optional<T> pop_latest() {
        std::optional<T> latest;
        while (auto v = try_pop()) {
            latest = std::move(v);
        }
        return latest;
    }

    [[nodiscard]] std::size_t approx_size() const {
        const auto h = head_.load(std::memory_order_acquire);
        const auto t = tail_.load(std::memory_order_acquire);
        return (h - t) & mask_;
    }

    [[nodiscard]] bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

private:
    static constexpr std::size_t mask_ = Capacity - 1;
    T slots_[Capacity]{};
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

}  // namespace vg
