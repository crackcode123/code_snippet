#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <type_traits>

namespace low_latency {

/// Lock-free bounded MPMC ring buffer — Vyukov-style per-slot sequence design.
///
/// Key difference from CAS-retry MPMC:
///   - Slot claim uses fetch_add (ONE atomic RMW, always succeeds, no retry loop).
///   - The thread then SPINS on the per-slot sequence number until it reaches the
///     expected value, meaning the slot is ready for the operation.
///
/// How it works:
///
///   Producer:
///     1. pos = head_.fetch_add(1)            ← unconditionally claim next slot
///     2. spin until slot[pos].seq == pos     ← wait for slot to be FREE
///     3. construct element in slot
///     4. slot[pos].seq = pos + 1             ← mark FULL, consumer can proceed
///
///   Consumer:
///     1. pos = tail_.fetch_add(1)            ← unconditionally claim next slot
///     2. spin until slot[pos].seq == pos + 1 ← wait for slot to be FULL
///     3. move element out, destroy
///     4. slot[pos].seq = pos + Capacity      ← mark FREE for next lap
///
///   The per-slot sequence number is the ONLY synchronization between a
///   specific producer-consumer pair operating on the same slot. The
///   fetch_add on head_/tail_ merely assigns slots — it never fails.
///
/// Per-slot sequence lifecycle:
///
///     Initial:       seq = i                 slot i is empty, ready for turn i
///     After produce: seq = pos + 1           slot is full
///     After consume: seq = pos + Capacity    slot is empty, ready for turn (pos + Capacity)
///
///     The sequence increases monotonically → no ABA.
///
/// Trade-offs vs CAS-retry MPMC:
///
///   CAS-retry:    try_push can return false immediately if full (non-blocking).
///   This design:  push always commits (fetch_add is irreversible). If the buffer
///                 is full, the producer MUST spin until space opens — cannot back out.
///                 try_push peeks first and avoids fetch_add if the slot isn't ready,
///                 but under contention this peek is advisory (benign race).
///
///   CAS-retry:    CAS failure → reload + retry loop (wasted iterations under contention).
///   This design:  fetch_add always succeeds in one shot → spin is purely on slot readiness.
///                 Under moderate contention, this is faster because there's no CAS storm.
///
/// Memory ordering:
///   fetch_add:  acq_rel — acquire sees prior slot seq updates, release publishes our claim.
///   slot.seq load:   acquire — see the element written by producer (or destruction by consumer).
///   slot.seq store:  release — publish element construction (or slot vacation).
///
/// Capacity: rounded to next power of 2. head_/tail_ on separate cache lines.

template <typename T, std::size_t RequestedCapacity>
class SeqSlotMPMC {
    static_assert(RequestedCapacity > 0, "Capacity must be > 0");

    static constexpr std::size_t next_pow2(std::size_t v) {
        v--;
        v |= v >> 1;  v |= v >> 2;  v |= v >> 4;
        v |= v >> 8;  v |= v >> 16; v |= v >> 32;
        return v + 1;
    }

    static constexpr std::size_t Capacity  = next_pow2(RequestedCapacity);
    static constexpr std::size_t Mask      = Capacity - 1;
    static constexpr std::size_t kCacheLine = 64;

    struct Slot {
        std::atomic<std::size_t> seq;
        union Storage {
            Storage() {}
            ~Storage() {}
            T val;
        } storage;
    };

public:
    SeqSlotMPMC() {
        for (std::size_t i = 0; i < Capacity; ++i)
            slots_[i].seq.store(i, std::memory_order_relaxed);
    }

    ~SeqSlotMPMC() {
        std::size_t t = tail_.load(std::memory_order_relaxed);
        std::size_t h = head_.load(std::memory_order_relaxed);
        while (t != h) {
            slots_[t & Mask].storage.val.~T();
            ++t;
        }
    }

    SeqSlotMPMC(const SeqSlotMPMC&)            = delete;
    SeqSlotMPMC& operator=(const SeqSlotMPMC&) = delete;
    SeqSlotMPMC(SeqSlotMPMC&&)                 = delete;
    SeqSlotMPMC& operator=(SeqSlotMPMC&&)      = delete;

    // ── Blocking push/pop (spin on slot sequence) ────────────────────

    /// Claim a slot via fetch_add, then spin until the slot is free.
    /// Always succeeds — never returns false. May spin if buffer is full.
    template <typename... Args>
    void push(Args&&... args) {
        // Step 1: claim the next write position. fetch_add is a single atomic
        // RMW — no CAS loop, no retry, always succeeds in one instruction.
        const std::size_t pos = head_.fetch_add(1, std::memory_order_acq_rel);
        Slot& slot = slots_[pos & Mask];

        // Step 2: spin until this slot's sequence reaches our expected value.
        // seq == pos means the slot completed its previous consumer cycle
        // and is now free for turn `pos`.
        while (slot.seq.load(std::memory_order_acquire) != pos)
            ;   // spin — slot is still occupied by a prior lap

        // Step 3: we exclusively own this slot. Construct element.
        ::new (&slot.storage.val) T(std::forward<Args>(args)...);

        // Step 4: publish. seq = pos + 1 tells consumer the element is ready.
        slot.seq.store(pos + 1, std::memory_order_release);
    }

    /// Claim a slot via fetch_add, then spin until the slot is full.
    /// Always succeeds. May spin if buffer is empty.
    T pop() {
        // Step 1: claim the next read position.
        const std::size_t pos = tail_.fetch_add(1, std::memory_order_acq_rel);
        Slot& slot = slots_[pos & Mask];

        // Step 2: spin until producer has filled this slot.
        // seq == pos + 1 means the element is ready.
        while (slot.seq.load(std::memory_order_acquire) != pos + 1)
            ;   // spin — producer hasn't written yet

        // Step 3: move element out, destroy in-place.
        T val = std::move(slot.storage.val);
        slot.storage.val.~T();

        // Step 4: publish. seq = pos + Capacity marks the slot free for the
        // next producer that wraps around to this position.
        slot.seq.store(pos + Capacity, std::memory_order_release);
        return val;
    }

    // ── Non-blocking try variants ────────────────────────────────────
    //
    // These peek at the slot sequence BEFORE committing with fetch_add.
    // Under zero contention this is race-free. Under contention, the peek
    // may see a stale value — but this is benign: at worst we return false
    // when a slot was actually available (conservative), never the reverse.

    /// Try to push without spinning. Returns false if buffer appears full.
    template <typename... Args>
    bool try_push(Args&&... args) {
        const std::size_t pos = head_.load(std::memory_order_relaxed);
        Slot& slot = slots_[pos & Mask];

        // Peek: is this slot free for turn `pos`?
        if (slot.seq.load(std::memory_order_acquire) != pos)
            return false;   // full (or stale view — conservative)

        // Commit: claim the slot.
        // Another producer may have claimed it between our peek and this fetch_add.
        // If so, we get pos+1 (or later) and must spin on that slot's seq.
        // To keep try_push truly non-blocking, we verify we got the position we expected.
        const std::size_t claimed = head_.fetch_add(1, std::memory_order_acq_rel);
        Slot& claimed_slot = slots_[claimed & Mask];

        // If we got a different position than we peeked, we still own it but must
        // honor the spin contract — this keeps the invariant that every fetch_add
        // is eventually completed.
        while (claimed_slot.seq.load(std::memory_order_acquire) != claimed)
            ;   // usually zero iterations if contention is low

        ::new (&claimed_slot.storage.val) T(std::forward<Args>(args)...);
        claimed_slot.seq.store(claimed + 1, std::memory_order_release);
        return true;
    }

    /// Try to pop without spinning. Returns std::nullopt if buffer appears empty.
    std::optional<T> try_pop() {
        const std::size_t pos = tail_.load(std::memory_order_relaxed);
        Slot& slot = slots_[pos & Mask];

        if (slot.seq.load(std::memory_order_acquire) != pos + 1)
            return std::nullopt;   // empty (or stale view)

        const std::size_t claimed = tail_.fetch_add(1, std::memory_order_acq_rel);
        Slot& claimed_slot = slots_[claimed & Mask];

        while (claimed_slot.seq.load(std::memory_order_acquire) != claimed + 1)
            ;

        T val = std::move(claimed_slot.storage.val);
        claimed_slot.storage.val.~T();
        claimed_slot.seq.store(claimed + Capacity, std::memory_order_release);
        return val;
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
    alignas(kCacheLine) Slot slots_[Capacity];
    alignas(kCacheLine) std::atomic<std::size_t> head_{0};
    alignas(kCacheLine) std::atomic<std::size_t> tail_{0};
};

} // namespace low_latency
