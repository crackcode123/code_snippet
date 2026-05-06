#pragma once

#include <array>
#include <cstddef>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <chrono>

namespace low_latency {

/// Thread-safe bounded ring buffer (MPMC) using mutex + condition variables.
///
/// - Multiple producers, multiple consumers.
/// - push() blocks when full until space is available.
/// - pop() blocks when empty until an element is available.
/// - try_push / try_pop return immediately.
/// - try_push_for / try_pop_for accept a timeout.
///
/// Capacity is fixed at compile time to avoid heap allocation on the hot path.
template <typename T, std::size_t Capacity>
class MutexRingBuffer {
    static_assert(Capacity > 0, "Capacity must be > 0");

public:
    MutexRingBuffer() = default;

    // Non-copyable, non-movable (contains mutex + condvars).
    MutexRingBuffer(const MutexRingBuffer&) = delete;
    MutexRingBuffer& operator=(const MutexRingBuffer&) = delete;
    MutexRingBuffer(MutexRingBuffer&&) = delete;
    MutexRingBuffer& operator=(MutexRingBuffer&&) = delete;

    // ── Blocking operations ──────────────────────────────────────────

    /// Block until space is available, then emplace the element.
    template <typename... Args>
    void push(Args&&... args) {
        std::unique_lock<std::mutex> lock(mtx_);
        not_full_.wait(lock, [this] { return count_ < Capacity; });
        emplace_at(head_, std::forward<Args>(args)...);
        advance(head_);
        ++count_;
        lock.unlock();
        not_empty_.notify_one();
    }

    /// Block until an element is available, then move it out.
    T pop() {
        std::unique_lock<std::mutex> lock(mtx_);
        not_empty_.wait(lock, [this] { return count_ > 0; });
        T val = std::move(buf_[tail_]);
        buf_[tail_].~T();
        advance(tail_);
        --count_;
        lock.unlock();
        not_full_.notify_one();
        return val;
    }

    // ── Non-blocking operations ──────────────────────────────────────

    /// Try to push without blocking. Returns false if full.
    template <typename... Args>
    bool try_push(Args&&... args) {
        std::lock_guard<std::mutex> lock(mtx_);
        if (count_ == Capacity) return false;
        emplace_at(head_, std::forward<Args>(args)...);
        advance(head_);
        ++count_;
        not_empty_.notify_one();
        return true;
    }

    /// Try to pop without blocking. Returns std::nullopt if empty.
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (count_ == 0) return std::nullopt;
        T val = std::move(buf_[tail_]);
        buf_[tail_].~T();
        advance(tail_);
        --count_;
        not_full_.notify_one();
        return val;
    }

    // ── Timed operations ─────────────────────────────────────────────

    /// Block up to `timeout` for space. Returns false on timeout.
    template <typename Rep, typename Period, typename... Args>
    bool try_push_for(const std::chrono::duration<Rep, Period>& timeout, Args&&... args) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!not_full_.wait_for(lock, timeout, [this] { return count_ < Capacity; }))
            return false;
        emplace_at(head_, std::forward<Args>(args)...);
        advance(head_);
        ++count_;
        lock.unlock();
        not_empty_.notify_one();
        return true;
    }

    /// Block up to `timeout` for an element. Returns std::nullopt on timeout.
    template <typename Rep, typename Period>
    std::optional<T> try_pop_for(const std::chrono::duration<Rep, Period>& timeout) {
        std::unique_lock<std::mutex> lock(mtx_);
        if (!not_empty_.wait_for(lock, timeout, [this] { return count_ > 0; }))
            return std::nullopt;
        T val = std::move(buf_[tail_]);
        buf_[tail_].~T();
        advance(tail_);
        --count_;
        lock.unlock();
        not_full_.notify_one();
        return val;
    }

    // ── Observers (lock-free reads are NOT safe; these acquire the lock) ─

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return count_;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return count_ == 0;
    }

    bool full() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return count_ == Capacity;
    }

    static constexpr std::size_t capacity() { return Capacity; }

private:
    /// Construct T in-place at the given slot using placement new.
    template <typename... Args>
    void emplace_at(std::size_t idx, Args&&... args) {
        ::new (&buf_[idx]) T(std::forward<Args>(args)...);
    }

    static void advance(std::size_t& idx) {
        if (++idx == Capacity) idx = 0;     // branchless: idx = (idx + 1) % Capacity
    }

    // Storage: uninitialized to avoid default-constructing Capacity elements.
    // Elements are constructed on push and destroyed on pop.
    union Slot {
        Slot() {}           // no-op: leaves storage uninitialized
        ~Slot() {}          // no-op: destruction handled manually in pop
        T val;
    };

    // Aligned raw storage that avoids requiring T to be default-constructible.
    alignas(T) Slot buf_[Capacity];

    std::size_t head_{0};       // next write position
    std::size_t tail_{0};       // next read position
    std::size_t count_{0};      // current number of elements

    mutable std::mutex mtx_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
};

} // namespace low_latency
