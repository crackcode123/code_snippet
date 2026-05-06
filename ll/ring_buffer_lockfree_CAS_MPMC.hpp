#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <type_traits>

namespace low_latency {

/// Lock-free bounded MPMC ring buffer using CAS (Compare-And-Swap) loops.
///
/// Design:
///   Multiple producers race to claim head_ via compare_exchange_weak.
///   Multiple consumers race to claim tail_ via compare_exchange_weak.
///   The CAS winner "owns" that slot exclusively — no lock needed.
///
///   Each slot has a per-slot atomic sequence number that acts as a state
///   machine for that slot's lifecycle:
///
///     seq == slot_index          → slot is EMPTY, ready for a producer
///     seq == slot_index + 1      → slot is FULL,  ready for a consumer
///     seq == slot_index + Capacity  → slot has been consumed, wrapping around
///
///   This per-slot sequence prevents ABA problems and provides a wait-free
///   fast path when the buffer is neither full nor empty.
///
/// Memory ordering:
///   - head_ / tail_ CAS: acq_rel (acquire on success to read slot seq,
///     release to publish the claimed index to other threads).
///   - Slot seq load: acquire (see the element written by the producer).
///   - Slot seq store: release (publish the element / slot vacation).
///
///   On ARM: CAS compiles to LDAXR/STLXR loop. Slot seq uses LDAR/STLR.
///   On x86: CAS compiles to LOCK CMPXCHG. Seq loads/stores are plain MOV
///           (x86 TSO gives acquire/release for free).
///
/// Capacity is rounded to next power of 2 for bitmask wrap.
/// Cache lines: head_ and tail_ are on separate cache lines.

template <typename T, std::size_t RequestedCapacity>
class LockfreeCAS_MPMC {
    static_assert(RequestedCapacity > 0, "Capacity must be > 0");

    static constexpr std::size_t next_pow2(std::size_t v) {
        v--;
        v |= v >> 1;  v |= v >> 2;  v |= v >> 4;
        v |= v >> 8;  v |= v >> 16; v |= v >> 32;
        return v + 1;
    }

    static constexpr std::size_t Capacity = next_pow2(RequestedCapacity);
    static constexpr std::size_t Mask     = Capacity - 1;
    static constexpr std::size_t kCacheLine = 64;

    // ── Per-slot state machine ───────────────────────────────────────
    //
    // Each slot carries a sequence number that tracks its lifecycle.
    // This eliminates the ABA problem that a naive CAS-only approach
    // on head_/tail_ would suffer from.
    //
    //   Producer sees slot.seq == pos      → slot is free, write element, set seq = pos + 1
    //   Consumer sees slot.seq == pos + 1  → slot is full, read element,  set seq = pos + Capacity
    //
    // The sequence number monotonically increases, so a stale CAS on
    // head_/tail_ that succeeds will still find the slot in the wrong
    // state and retry — no data corruption.

    struct Slot {
        std::atomic<std::size_t> seq;
        // Uninitialized storage for T.
        union Storage {
            Storage() {}
            ~Storage() {}
            T val;
        } storage;
    };

public:
    LockfreeCAS_MPMC() {
        // Initialize each slot's sequence to its index.
        // seq[i] == i means "slot i is empty and ready for turn i."
        for (std::size_t i = 0; i < Capacity; ++i)
            slots_[i].seq.store(i, std::memory_order_relaxed);
    }

    ~LockfreeCAS_MPMC() {
        // Destroy any elements still in the buffer.
        std::size_t h = head_.load(std::memory_order_relaxed);
        std::size_t t = tail_.load(std::memory_order_relaxed);
        while (t != h) {
            Slot& slot = slots_[t & Mask];
            slot.storage.val.~T();
            ++t;
        }
    }

    // Non-copyable, non-movable.
    LockfreeCAS_MPMC(const LockfreeCAS_MPMC&)            = delete;
    LockfreeCAS_MPMC& operator=(const LockfreeCAS_MPMC&) = delete;
    LockfreeCAS_MPMC(LockfreeCAS_MPMC&&)                 = delete;
    LockfreeCAS_MPMC& operator=(LockfreeCAS_MPMC&&)      = delete;

    // ── Producer API (any thread) ────────────────────────────────────

    /// Try to emplace an element. Returns false if full.
    ///
    /// Algorithm:
    ///   1. Load current head_ (our "pos" candidate).
    ///   2. Look at slots_[pos & Mask].seq.
    ///      - If seq == pos: slot is free at this turn. Try to CAS head_ from pos to pos+1.
    ///        * CAS succeeds: we own slot `pos`. Construct element. Set seq = pos + 1.
    ///        * CAS fails: another producer won. Reload pos, retry.
    ///      - If seq < pos (diff < 0): buffer is full (consumer hasn't freed this slot yet).
    ///        Return false.
    ///      - If seq > pos: another producer already claimed and filled this slot,
    ///        but head_ hasn't been updated yet in our view. Reload and retry.
    template <typename... Args>
    bool try_push(Args&&... args) {
        std::size_t pos = head_.load(std::memory_order_relaxed);

        for (;;) {
            Slot& slot = slots_[pos & Mask];
            std::size_t seq = slot.seq.load(std::memory_order_acquire);
            auto diff = static_cast<std::ptrdiff_t>(seq - pos);

            if (diff == 0) {
                // Slot is free at this turn. Race to claim it.
                if (head_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed)) {
                    // Won the slot. Construct element in-place.
                    ::new (&slot.storage.val) T(std::forward<Args>(args)...);
                    // Publish: set seq = pos + 1 so consumers can see it.
                    slot.seq.store(pos + 1, std::memory_order_release);
                    return true;
                }
                // CAS failed — pos has been reloaded by compare_exchange_weak.
                // Loop again with the new pos.
            } else if (diff < 0) {
                // seq < pos: consumer hasn't freed this slot yet → buffer full.
                return false;
            } else {
                // diff > 0: another producer grabbed this slot but we see a stale head_.
                // Reload head_ and retry.
                pos = head_.load(std::memory_order_relaxed);
            }
        }
    }

    /// Spin-push: busy-wait until space is available.
    template <typename... Args>
    void push(Args&&... args) {
        while (!try_push(std::forward<Args>(args)...))
            ;
    }

    // ── Consumer API (any thread) ────────────────────────────────────

    /// Try to pop an element. Returns std::nullopt if empty.
    ///
    /// Algorithm (mirror of try_push):
    ///   1. Load current tail_ (our "pos" candidate).
    ///   2. Look at slots_[pos & Mask].seq.
    ///      - If seq == pos + 1: slot is full at this turn. CAS tail_ from pos to pos+1.
    ///        * CAS succeeds: we own the slot. Move element out. Set seq = pos + Capacity.
    ///        * CAS fails: another consumer won. Reload, retry.
    ///      - If seq < pos + 1 (diff < 0): buffer is empty. Return nullopt.
    ///      - If seq > pos + 1: stale tail_ view. Reload, retry.
    std::optional<T> try_pop() {
        std::size_t pos = tail_.load(std::memory_order_relaxed);

        for (;;) {
            Slot& slot = slots_[pos & Mask];
            std::size_t seq = slot.seq.load(std::memory_order_acquire);
            auto diff = static_cast<std::ptrdiff_t>(seq - (pos + 1));

            if (diff == 0) {
                // Slot is full at this turn. Race to claim it.
                if (tail_.compare_exchange_weak(pos, pos + 1,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed)) {
                    // Won the slot. Move element out and destroy.
                    T val = std::move(slot.storage.val);
                    slot.storage.val.~T();
                    // Publish: set seq = pos + Capacity so producers can reuse this slot
                    // on the next lap around the ring.
                    slot.seq.store(pos + Capacity, std::memory_order_release);
                    return val;
                }
                // CAS failed — pos reloaded. Retry.
            } else if (diff < 0) {
                // seq < pos + 1: producer hasn't filled this slot yet → buffer empty.
                return std::nullopt;
            } else {
                // Stale tail_ view. Reload.
                pos = tail_.load(std::memory_order_relaxed);
            }
        }
    }

    /// Spin-pop: busy-wait until an element is available.
    T pop() {
        for (;;) {
            if (auto v = try_pop())
                return *std::move(v);
        }
    }

    // ── Observers ────────────────────────────────────────────────────

    std::size_t size_approx() const {
        std::size_t h = head_.load(std::memory_order_acquire);
        std::size_t t = tail_.load(std::memory_order_acquire);
        return (h >= t) ? (h - t) : 0;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    static constexpr std::size_t capacity() { return Capacity; }

private:
    // ── Data layout ──────────────────────────────────────────────────
    //
    //  slots_[] :  bulk of the memory, cache-line aligned
    //  head_    :  own cache line (producers contend here)
    //  tail_    :  own cache line (consumers contend here)
    //
    //  head_ and tail_ are monotonically increasing counters (NOT masked).
    //  Masking is applied when indexing into slots_[].
    //  This avoids the ABA issue that would arise if head_ wrapped to 0.

    alignas(kCacheLine) Slot slots_[Capacity];
    alignas(kCacheLine) std::atomic<std::size_t> head_{0};
    alignas(kCacheLine) std::atomic<std::size_t> tail_{0};
};

} // namespace low_latency
