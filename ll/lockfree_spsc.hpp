#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <type_traits>

namespace low_latency {

/// Lock-free bounded SPSC (Single Producer, Single Consumer) ring buffer.
///
/// Design:
///   - head_ (write index) lives on its own cache line, owned by the producer.
///   - tail_ (read index) lives on its own cache line, owned by the consumer.
///   - Cache-line padding between them prevents false sharing.
///   - Capacity is rounded up to the next power of 2 so the index wrap
///     is a cheap bitwise AND instead of a branch or modulo.
///   - Memory ordering: acquire/release on the atomic indices is sufficient
///     for correctness on weakly-ordered architectures (ARM, POWER).
///     No seq_cst, no fences, no locks.
///
/// Memory ordering rationale (ARM / weakly-ordered):
///
///   Producer:
///     1. Writes element to buf_[head]             (plain store via placement new)
///     2. head_.store(next, release)               ← publishes the write
///        The release guarantees that the element write in (1) is visible
///        to any thread that observes the updated head via acquire load.
///
///   Consumer:
///     1. head_.load(acquire)                      ← synchronizes-with producer's release
///        The acquire guarantees that all writes the producer did before
///        the release (i.e., the element store) are visible here.
///     2. Reads element from buf_[tail]            (now safe)
///     3. tail_.store(next, release)               ← publishes the consumption
///
///   Producer reading tail_ uses acquire for the same reason in reverse:
///     it must see the consumer's prior destruction/move of the old element
///     before reusing that slot.
///
///   On x86 (TSO): acquire/release compile to plain MOV (free).
///   On ARM:       acquire compiles to LDAR, release to STLR — minimal overhead.
///   Using seq_cst would add unnecessary DMB (ARM) or MFENCE (x86).

template <typename T, std::size_t RequestedCapacity>
class LockfreeSPSC {
    static_assert(RequestedCapacity > 0, "Capacity must be > 0");

    // Round up to next power of 2 so wrap = (idx + 1) & Mask instead of modulo.
    static constexpr std::size_t next_pow2(std::size_t v) {
        v--;
        v |= v >> 1;  v |= v >> 2;  v |= v >> 4;
        v |= v >> 8;  v |= v >> 16; v |= v >> 32;
        return v + 1;
    }

    static constexpr std::size_t Capacity = next_pow2(RequestedCapacity);
    static constexpr std::size_t Mask     = Capacity - 1;

    // ── Cache-line isolation ─────────────────────────────────────────
    //
    // Layout (assuming 64-byte cache line):
    //
    //   Cache line 1:  [ head_ (8 bytes) ][ 56 bytes padding ]
    //   Cache line 2:  [ tail_ (8 bytes) ][ 56 bytes padding ]
    //
    // The producer writes head_ and reads tail_.
    // The consumer writes tail_ and reads head_.
    // Without padding, both atomics could sit on the same cache line
    // and every write by either side would invalidate the other's
    // cache line (false sharing), adding ~40-80 ns per operation.

    static constexpr std::size_t kCacheLine = 64;

    // Uninitialized slots: T is not required to be default-constructible.
    union Slot {
        Slot() {}           // no-op: leaves storage uninitialized
        ~Slot() {}          // no-op: destruction handled manually
        T val;
    };

public:
    LockfreeSPSC() = default;

    ~LockfreeSPSC() {
        // Destroy any elements remaining in the buffer.
        std::size_t t = tail_.load(std::memory_order_relaxed);
        std::size_t h = head_.load(std::memory_order_relaxed);
        while (t != h) {
            buf_[t].val.~T();
            t = (t + 1) & Mask;
        }
    }

    // Non-copyable, non-movable.
    LockfreeSPSC(const LockfreeSPSC&)            = delete;
    LockfreeSPSC& operator=(const LockfreeSPSC&) = delete;
    LockfreeSPSC(LockfreeSPSC&&)                 = delete;
    LockfreeSPSC& operator=(LockfreeSPSC&&)      = delete;

    // ── Producer API (call from ONE producer thread only) ────────────

    /// Try to emplace an element. Returns false if full.
    template <typename... Args>
    bool try_push(Args&&... args) {
        // Producer owns head_ — relaxed load is safe (no other writer).
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = (head + 1) & Mask;

        // Full check: next write position == current tail means full.
        // acquire on tail_: synchronize-with consumer's release store
        // so we see that the slot at `next` has been vacated (element destroyed).
        if (next == tail_.load(std::memory_order_acquire))
            return false;

        // Construct element in-place via placement new.
        ::new (&buf_[head].val) T(std::forward<Args>(args)...);

        // Publish: release ensures the element construction above is visible
        // to the consumer before it sees the updated head.
        head_.store(next, std::memory_order_release);
        return true;
    }

    /// Spin-push: busy-wait until space is available, then push.
    /// Use only when latency matters more than CPU usage.
    template <typename... Args>
    void push(Args&&... args) {
        while (!try_push(std::forward<Args>(args)...))
            ;   // spin — consider _mm_pause() / __yield() for hyper-threading
    }

    // ── Consumer API (call from ONE consumer thread only) ────────────

    /// Try to pop an element. Returns std::nullopt if empty.
    std::optional<T> try_pop() {
        // Consumer owns tail_ — relaxed load is safe (no other writer).
        const std::size_t tail = tail_.load(std::memory_order_relaxed);

        // Empty check: tail == head means nothing to read.
        // acquire on head_: synchronize-with producer's release store
        // so we see the element that was constructed at buf_[tail].
        if (tail == head_.load(std::memory_order_acquire))
            return std::nullopt;

        // Move element out, then destroy the in-place copy.
        T val = std::move(buf_[tail].val);
        buf_[tail].val.~T();

        // Publish: release ensures the destruction above completes before
        // the producer sees the updated tail and reuses the slot.
        tail_.store((tail + 1) & Mask, std::memory_order_release);
        return val;
    }

    /// Spin-pop: busy-wait until an element is available.
    T pop() {
        for (;;) {
            if (auto v = try_pop())
                return *std::move(v);
        }
    }

    /// Peek at the front element without removing it. Returns nullptr if empty.
    /// Consumer thread only.
    T* front() {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
            return nullptr;
        return &buf_[tail].val;
    }

    // ── Observers ────────────────────────────────────────────────────

    /// Approximate size (snapshot — may be stale).
    std::size_t size_approx() const {
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        return (head - tail) & Mask;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    static constexpr std::size_t capacity() { return Capacity; }

private:
    // ── Data members, laid out for cache-line isolation ───────────────

    alignas(kCacheLine) Slot buf_[Capacity];

    // head_: written by producer, read by consumer.
    // Sits on its own cache line to avoid false sharing with tail_.
    alignas(kCacheLine) std::atomic<std::size_t> head_{0};

    // tail_: written by consumer, read by producer.
    // Sits on its own cache line to avoid false sharing with head_.
    alignas(kCacheLine) std::atomic<std::size_t> tail_{0};
};

} // namespace low_latency
