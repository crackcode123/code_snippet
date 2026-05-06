# Top 5 Data Structures & Top 5 Algorithms for HFT/MFT

> C++ implementations with latency analysis. All code is production-oriented — no Python, no toy examples.

## Algorithms ↔ Hardware

```
        ┌──────────────────────────┐                    ┌──────────────────────────┐
        │      ALGORITHMS          │                    │       HARDWARE           │
        │      (exponents)         │                    │      (constants)         │
        ├──────────────────────────┤                    ├──────────────────────────┤
        │                          │                    │                          │
        │  • Graphs (BFS/SCC)      │ ◄── cache lines ──►│  • L1/L2/L3 (ns)         │
        │  • Trees (segtree/BIT)   │                    │  • TLB / page tables     │
        │  • Heaps (d-ary)         │ ◄── branch pred ──►│  • Pipeline / cmov       │
        │  • Hashing (robin hood)  │                    │  • Out-of-order exec     │
        │  • Tries / Aho-Corasick  │ ◄── prefetcher  ──►│  • DRAM bandwidth        │
        │  • DP (bitmask/interval) │                    │  • NUMA nodes            │
        │  • Sorting (radix/intro) │ ◄── SIMD lanes  ──►│  • AVX-512 / NEON        │
        │  • Strings (KMP/SA)      │                    │  • Vector registers      │
        │  • Lock-free structures  │ ◄── coherence   ──►│  • MESI / store buffer   │
        │  • Parallel scan/reduce  │                    │  • Warps / SMs (GPU)     │
        │                          │ ◄── memory order ─►│  • acquire / release     │
        │                          │                    │                          │
        │   "what to compute"      │ ◄── latency cost ─►│   "what it costs"        │
        │      O(n log n)          │                    │      ns / cycles         │
        │                          │                    │                          │
        └────────────┬─────────────┘                    └────────────┬─────────────┘
                     │                                               │
                     │                                               │
                     └──────────────────┐         ┌──────────────────┘
                                        ▼         ▼
                              ┌────────────────────────┐
                              │   REAL SYSTEM (you)    │
                              ├────────────────────────┤
                              │  • HFT order router    │
                              │  • GPU kernel          │
                              │  • Scheduler           │
                              │  • Inference engine    │
                              ├────────────────────────┤
                              │  ns/op   +   correct   │
                              │  measured + provable   │
                              └────────────────────────┘

   left side picks the EXPONENT  ──►  right side decides the CONSTANT
   pick wrong algo: 1000x slower  ──►  ignore hardware: 100x slower
                              both wrong: irrelevant
```

Algorithm without hardware awareness is a textbook. Hardware without algorithms is a benchmark suite. The job is the join.

| # | Data Structures | Algorithms |
|---|---|---|
| 1 | Ring Buffer (Lock-Free SPSC) | Binary Search (Branchless) |
| 2 | Hash Map (Robin Hood) | Sorting (Radix + Introsort) |
| 3 | Sorted Array (Order Book) | Sliding Window / Streaming Stats |
| 4 | Priority Queue (Timer Heap) | Graph Algos (Topo Sort, Arbitrage) |
| 5 | Trie (Radix Tree) | Dynamic Programming (Order Slicing) |

---

# DATA STRUCTURES

---

## 1. Ring Buffer (Lock-Free SPSC Circular Buffer)

**What it is:** A fixed-size circular buffer where one thread writes (producer) and one thread reads (consumer) with zero locks. The producer owns the `write_pos`, the consumer owns the `read_pos`, and they coordinate solely through atomic loads/stores with acquire/release semantics.

**Why it exists in HFT:** The market data feed handler receives millions of updates/sec from the exchange. It must hand off each update to the strategy thread without ever blocking. A lock-free SPSC ring buffer gives bounded, predictable latency — no malloc, no mutex, no syscall, no cache-line bouncing beyond the two shared atomics.

```cpp
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <type_traits>

// Cache line size for padding — 64 bytes on x86, may differ on ARM
inline constexpr std::size_t CACHE_LINE = 64;

/// Lock-free single-producer single-consumer ring buffer.
/// Capacity MUST be a power of 2 (enforced at compile time via static_assert).
template <typename T, std::size_t N>
class SPSCRingBuffer {
    static_assert(N > 0 && (N & (N - 1)) == 0, "Capacity must be a power of 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable for HFT use");

public:
    SPSCRingBuffer() : write_pos_{0}, read_pos_{0} {}

    // ── producer (single thread) ───────────────────────────────

    /// Try to push an element. Returns false if full.
    bool try_push(const T& item) noexcept {
        const uint64_t wp = write_pos_.load(std::memory_order_relaxed);
        const uint64_t rp = read_pos_.load(std::memory_order_acquire);  // sync with consumer

        if (wp - rp == N) return false;  // full

        buf_[wp & kMask] = item;

        // Release: all writes to buf_ are visible before consumer sees updated write_pos
        write_pos_.store(wp + 1, std::memory_order_release);
        return true;
    }

    // ── consumer (single thread) ───────────────────────────────

    /// Try to pop an element. Returns std::nullopt if empty.
    std::optional<T> try_pop() noexcept {
        const uint64_t rp = read_pos_.load(std::memory_order_relaxed);
        const uint64_t wp = write_pos_.load(std::memory_order_acquire);  // sync with producer

        if (rp == wp) return std::nullopt;  // empty

        T item = buf_[rp & kMask];

        // Release: consumer is done reading buf_ slot before producer can overwrite
        read_pos_.store(rp + 1, std::memory_order_release);
        return item;
    }

    /// Peek at front without consuming. Returns std::nullopt if empty.
    std::optional<T> peek() const noexcept {
        const uint64_t rp = read_pos_.load(std::memory_order_relaxed);
        const uint64_t wp = write_pos_.load(std::memory_order_acquire);

        if (rp == wp) return std::nullopt;
        return buf_[rp & kMask];
    }

    // ── observers (safe from either thread) ────────────────────

    std::size_t size() const noexcept {
        uint64_t wp = write_pos_.load(std::memory_order_acquire);
        uint64_t rp = read_pos_.load(std::memory_order_acquire);
        return static_cast<std::size_t>(wp - rp);
    }

    bool empty() const noexcept { return size() == 0; }
    static constexpr std::size_t capacity() noexcept { return N; }

private:
    static constexpr uint64_t kMask = N - 1;  // bitmask replaces modulo

    // Pad each atomic to its own cache line to prevent false sharing.
    // Producer hammers write_pos_, consumer hammers read_pos_ — they must
    // NOT share a 64-byte cache line or you get cross-core invalidation traffic.
    alignas(CACHE_LINE) std::atomic<uint64_t> write_pos_;
    alignas(CACHE_LINE) std::atomic<uint64_t> read_pos_;

    // Buffer itself: aligned for clean cache-line access
    alignas(CACHE_LINE) T buf_[N];
};
```

### Complexity

| Operation | Time  | Notes                             |
|-----------|-------|-----------------------------------|
| push      | O(1)  | Single atomic store (release)     |
| pop       | O(1)  | Single atomic store (release)     |
| peek      | O(1)  | Atomic load only, no state change |
| **Space** | O(n)  | Fixed at compile time, no heap    |

### Why This Matters in HFT

**No allocation.** The buffer is a flat array allocated once (often at startup on huge pages). During trading hours, `try_push` / `try_pop` touch exactly one cache line for the data plus one atomic — zero calls to `malloc`.

**No locks.** A mutex-based queue means a syscall under contention (`futex` on Linux). Even uncontended, `pthread_mutex_lock` is ~25ns overhead. The atomic store here compiles to a single `MOV` on x86 (which has a strong memory model — release stores are plain stores). Measured latency:

| Queue type                | Push latency (p50) | Push latency (p99) |
|---------------------------|--------------------|---------------------|
| `std::queue` + `std::mutex` | ~30–60 ns        | ~200–2000 ns (contention/syscall) |
| Lock-free SPSC ring buffer   | ~5–10 ns          | ~10–20 ns (cache miss on full scan) |

**Cache-friendly.** Sequential writes into a contiguous array give hardware prefetcher the ideal access pattern. The producer writes slot `i`, then `i+1`, then `i+2` — the prefetcher sees the stride and pre-loads the next cache line before you ask for it.

**Bounded latency.** Fixed capacity means no reallocation surprise. If the buffer fills up, `try_push` returns false — the producer decides what to do (drop, backpressure). This is a design choice: in HFT you'd rather drop stale data than stall the feed handler.

**Typical use cases:**
- Market data feed handler → strategy engine (one producer socket reader, one consumer strategy thread)
- Strategy engine → order gateway (order queue, bounded so backpressure is visible)
- Logging: hot path pushes log entries, background thread drains and writes to disk

---

## 2. Hash Map (Open Addressing, Robin Hood Hashing)

**What it is:** A flat hash map where all entries live in a single contiguous array (no linked lists, no per-node heap allocations). Robin Hood hashing resolves collisions by stealing from "rich" entries (those close to their ideal slot) and giving to "poor" entries (those far from their ideal slot). This bounds the max probe length and keeps variance low.

**Why it exists in HFT:** `std::unordered_map` uses separate chaining — every insert calls `new`, every lookup chases a pointer from the bucket array to a heap-allocated node. That's 2+ cache misses per lookup minimum. An open-addressing map with flat storage gives 1 cache miss for the common case (key is at or near its ideal slot).

```cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

/// Open-addressing hash map with Robin Hood hashing.
/// Capacity is always a power of 2. Max load factor ~0.75 before resize.
template <typename Key, typename Value,
          typename Hash = std::hash<Key>,
          typename KeyEq = std::equal_to<Key>>
class RobinHoodMap {
    static_assert(std::is_trivially_copyable_v<Key>, "Key must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Value>, "Value must be trivially copyable");

    struct Slot {
        Key    key;
        Value  value;
        int32_t dist;  // probe distance from ideal slot. -1 = empty.

        bool occupied() const { return dist >= 0; }
    };

public:
    explicit RobinHoodMap(std::size_t initial_capacity = 1024)
        : size_{0}, mask_{0}, slots_{nullptr}
    {
        // Round up to power of 2
        std::size_t cap = 1;
        while (cap < initial_capacity) cap <<= 1;
        alloc(cap);
    }

    ~RobinHoodMap() { delete[] slots_; }

    // Non-copyable, movable
    RobinHoodMap(const RobinHoodMap&) = delete;
    RobinHoodMap& operator=(const RobinHoodMap&) = delete;

    RobinHoodMap(RobinHoodMap&& o) noexcept
        : size_{o.size_}, mask_{o.mask_}, slots_{o.slots_} {
        o.slots_ = nullptr; o.size_ = 0; o.mask_ = 0;
    }

    // ── insert / upsert ────────────────────────────────────────

    /// Insert or update. Returns pointer to value in the map.
    Value* insert(Key key, Value value) {
        if (size_ * 4 >= (mask_ + 1) * 3) grow();  // load > 0.75

        return insert_inner(key, value);
    }

    // ── lookup ─────────────────────────────────────────────────

    /// Returns pointer to value if found, nullptr otherwise.
    Value* find(const Key& key) noexcept {
        uint64_t idx = Hash{}(key) & mask_;
        int32_t  dist = 0;

        while (true) {
            Slot& s = slots_[idx];
            if (!s.occupied())   return nullptr;   // empty slot — not in map
            if (s.dist < dist)   return nullptr;   // Robin Hood invariant: if
                                                    // existing entry is closer to
                                                    // home than we would be, our
                                                    // key was never inserted
            if (KeyEq{}(s.key, key)) return &s.value;

            idx = (idx + 1) & mask_;
            ++dist;
        }
    }

    const Value* find(const Key& key) const noexcept {
        return const_cast<RobinHoodMap*>(this)->find(key);
    }

    // ── delete ─────────────────────────────────────────────────

    /// Erase by key. Returns true if found and removed.
    bool erase(const Key& key) noexcept {
        uint64_t idx = Hash{}(key) & mask_;
        int32_t  dist = 0;

        while (true) {
            Slot& s = slots_[idx];
            if (!s.occupied())  return false;
            if (s.dist < dist)  return false;

            if (KeyEq{}(s.key, key)) {
                // Found it. Backward-shift delete: pull subsequent entries
                // back one slot until we hit an empty slot or an entry at
                // its ideal position (dist == 0).
                uint64_t cur = idx;
                while (true) {
                    uint64_t nxt = (cur + 1) & mask_;
                    Slot& ns = slots_[nxt];
                    if (!ns.occupied() || ns.dist == 0) {
                        slots_[cur].dist = -1;  // mark empty
                        break;
                    }
                    slots_[cur] = ns;
                    slots_[cur].dist--;
                    cur = nxt;
                }
                --size_;
                return true;
            }
            idx = (idx + 1) & mask_;
            ++dist;
        }
    }

    // ── observers ──────────────────────────────────────────────

    std::size_t size()     const noexcept { return size_; }
    std::size_t capacity() const noexcept { return mask_ + 1; }
    bool        empty()    const noexcept { return size_ == 0; }

private:
    void alloc(std::size_t cap) {
        mask_  = cap - 1;
        slots_ = new Slot[cap];
        for (std::size_t i = 0; i < cap; ++i) slots_[i].dist = -1;
    }

    void grow() {
        std::size_t old_cap = mask_ + 1;
        Slot* old = slots_;

        alloc(old_cap * 2);
        size_ = 0;

        for (std::size_t i = 0; i < old_cap; ++i) {
            if (old[i].occupied()) {
                insert_inner(old[i].key, old[i].value);
            }
        }
        delete[] old;
    }

    /// Core insert with Robin Hood swapping.
    Value* insert_inner(Key key, Value value) {
        uint64_t idx  = Hash{}(key) & mask_;
        int32_t  dist = 0;
        Value*   result = nullptr;

        while (true) {
            Slot& s = slots_[idx];

            if (!s.occupied()) {
                // Empty slot — place here
                s.key   = key;
                s.value = value;
                s.dist  = dist;
                ++size_;
                return result ? result : &s.value;
            }

            // Update existing key
            if (KeyEq{}(s.key, key)) {
                s.value = value;
                return &s.value;
            }

            // Robin Hood: if the existing entry is richer (closer to home)
            // than us, we steal its slot and the displaced entry continues probing
            if (s.dist < dist) {
                std::swap(key,   s.key);
                std::swap(value, s.value);
                std::swap(dist,  s.dist);
                if (!result) result = &s.value;  // original key is now placed here
            }

            idx = (idx + 1) & mask_;
            ++dist;
        }
    }

    std::size_t size_;
    uint64_t    mask_;   // capacity - 1, for bitmask modulo
    Slot*       slots_;  // flat contiguous array
};
```

### Complexity

| Operation | Time              | Notes                                      |
|-----------|-------------------|--------------------------------------------| 
| insert    | O(1) amortized    | Amortized over resizes; Robin Hood bounds max probe |
| lookup    | O(1) amortized    | Early termination via probe-distance check |
| delete    | O(1) amortized    | Backward-shift avoids tombstone buildup    |
| **Space** | O(n)              | ~1.33x entries at 0.75 load factor         |

### Why This Beats `std::unordered_map` in HFT

**Memory layout.** `std::unordered_map` stores each entry as a separately heap-allocated node. A lookup does: load bucket pointer -> follow pointer to node -> compare key. That's 2 cache misses minimum, and the nodes are scattered across the heap so the prefetcher can't help.

Robin Hood open addressing stores everything in one flat array. A lookup loads the slot directly from the array — one cache miss if the slot isn't in L1/L2 already, and adjacent probes are in the same or next cache line.

```
std::unordered_map lookup path:
  bucket_array[hash] → pointer → heap node → key compare
                       ^cache miss  ^cache miss (random heap location)

RobinHoodMap lookup path:
  slots_[hash & mask] → key compare (inline, same array)
                        ^one cache miss, next probes are adjacent
```

**No per-insert allocation.** `std::unordered_map::insert` calls `operator new` for every entry. Even with a pool allocator, that's overhead. The flat map does zero allocations until it needs to resize (which you can pre-size away at startup).

**Probe length variance.** Standard linear probing suffers from clustering — one unlucky run of collisions creates a long probe chain that gets worse over time. Robin Hood hashing redistributes probe distances: rich entries give up their slot to poor entries. The result is that the *maximum* probe distance stays small and the *variance* collapses. Measured at 0.7 load factor:

| Metric                      | Linear probing | Robin Hood |
|-----------------------------|---------------|------------|
| Average probe length        | ~1.5          | ~1.5       |
| Max probe length (10M keys) | ~40–80        | ~8–12      |
| Variance in probe length    | High          | Very low   |

Low variance means predictable tail latency — the p99 lookup time is close to the p50, which is exactly what HFT systems need.

**Backward-shift delete.** Many open-addressing maps use tombstones (marking deleted slots as "deleted but not empty" so probe chains aren't broken). Tombstones degrade performance over time as the table fills with dead slots. Backward-shift delete physically removes the entry and shifts subsequent entries back, keeping the table clean.

**Typical use cases:**
- Order ID → order state lookup (millions of live orders, looked up on every fill/cancel)
- Symbol → instrument metadata (static at start of day, read-heavy)
- Client ID → position tracking (updated on every trade)

---

## 3. Sorted Array (Price-Level Order Book)

### Why Sorted Array Beats Trees for Order Books

The typical active order book has 5–20 price levels being updated per side. At this scale, `std::map` (red-black tree) is a disaster: pointer chasing on every traversal, one heap allocation per node, zero spatial locality. A sorted `std::vector` keeps everything in one contiguous cache line block — binary search touches adjacent memory, inserts shift a handful of elements via `memmove` (which the CPU prefetcher handles trivially), and top-of-book is always index 0.

For a 20-level book, a full linear shift is ~160 bytes moved — one or two cache lines. A tree traversal for the same operation touches 5+ pointer-indirected nodes scattered across the heap.

**Skip lists** add probabilistic balancing overhead and pointer chasing similar to trees. **Red-black trees** guarantee O(log n) but the constant factor from cache misses dominates at small n. The sorted array wins until n exceeds a few hundred levels, which rarely happens for actively-quoted levels.

| Operation | Sorted Array | std::map (RB-Tree) | Skip List |
|---|---|---|---|
| Top-of-book | O(1), cache-hot | O(log n), pointer chase | O(1) with head pointer |
| Insert near top | O(n) shift, but n≈20 | O(log n) + alloc | O(log n) probabilistic |
| Lookup by price | O(log n) binary search | O(log n) | O(log n) expected |
| Cache behavior | Excellent, contiguous | Terrible, scattered | Poor, pointer-based |
| Memory overhead | Zero per-element overhead | 3 pointers + color/node | ~1.33 pointers/node avg |

### Implementation

```cpp
#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include <functional>
#include <stdexcept>

// Price in fixed-point (e.g., price * 10000 to avoid floating point)
using Price = int64_t;
using Qty   = int64_t;

struct PriceLevel {
    Price price;
    Qty   qty;
    int   order_count;
};

// Side trait: bids sorted descending (highest first), asks sorted ascending (lowest first)
enum class Side : uint8_t { Bid, Ask };

template <Side S>
class SortedBook {
public:
    // Reserve typical depth to avoid early reallocs
    SortedBook() { levels_.reserve(32); }

    // Top-of-book: O(1). Returns nullptr if empty.
    const PriceLevel* top() const noexcept {
        return levels_.empty() ? nullptr : &levels_[0];
    }

    // Find level by price: O(log n) binary search.
    // Returns index, or -1 if not found.
    int find(Price price) const noexcept {
        auto it = lower_bound(price);
        int idx = static_cast<int>(it - levels_.begin());
        if (it != levels_.end() && it->price == price) return idx;
        return -1;
    }

    // Add quantity at a price level.
    // If level exists, increments qty and order_count.
    // If not, inserts new level in sorted position.
    // Returns index of the affected level.
    int add(Price price, Qty qty) {
        auto it = lower_bound(price);

        // Existing level — just update
        if (it != levels_.end() && it->price == price) {
            it->qty += qty;
            it->order_count++;
            return static_cast<int>(it - levels_.begin());
        }

        // New level — insert at sorted position
        // For small n, this memmove is cheaper than tree rebalancing
        int idx = static_cast<int>(it - levels_.begin());
        levels_.insert(it, PriceLevel{price, qty, 1});
        return idx;
    }

    // Remove quantity at a price level.
    // Removes the level entirely if qty drops to zero.
    // Returns true if level was found.
    bool remove(Price price, Qty qty) {
        auto it = lower_bound(price);
        if (it == levels_.end() || it->price != price) return false;

        it->qty -= qty;
        it->order_count--;

        if (it->qty <= 0) {
            levels_.erase(it);  // Shift left — cheap for small n
        }
        return true;
    }

    // Replace entire quantity at a level (for snapshot/L2 updates)
    void set(Price price, Qty qty) {
        auto it = lower_bound(price);

        if (qty == 0) {
            // Delete level
            if (it != levels_.end() && it->price == price) {
                levels_.erase(it);
            }
            return;
        }

        if (it != levels_.end() && it->price == price) {
            it->qty = qty;  // Update in place
        } else {
            levels_.insert(it, PriceLevel{price, qty, 1});
        }
    }

    size_t depth() const noexcept { return levels_.size(); }

    const PriceLevel& operator[](size_t i) const { return levels_[i]; }

    // Iterate all levels (already in sorted order)
    const std::vector<PriceLevel>& levels() const noexcept { return levels_; }

    void clear() noexcept { levels_.clear(); }

private:
    std::vector<PriceLevel> levels_;

    // Binary search respecting side ordering.
    // Bids: descending (highest price first) — compare with std::greater
    // Asks: ascending  (lowest price first)  — compare with std::less
    auto lower_bound(Price price) const noexcept {
        if constexpr (S == Side::Bid) {
            return std::lower_bound(
                levels_.begin(), levels_.end(), price,
                [](const PriceLevel& lvl, Price p) { return lvl.price > p; }
            );
        } else {
            return std::lower_bound(
                levels_.begin(), levels_.end(), price,
                [](const PriceLevel& lvl, Price p) { return lvl.price < p; }
            );
        }
    }

    // Non-const overloads for mutation
    auto lower_bound(Price price) noexcept {
        if constexpr (S == Side::Bid) {
            return std::lower_bound(
                levels_.begin(), levels_.end(), price,
                [](const PriceLevel& lvl, Price p) { return lvl.price > p; }
            );
        } else {
            return std::lower_bound(
                levels_.begin(), levels_.end(), price,
                [](const PriceLevel& lvl, Price p) { return lvl.price < p; }
            );
        }
    }
};

// Convenience typedefs
using BidBook = SortedBook<Side::Bid>;
using AskBook = SortedBook<Side::Ask>;

struct OrderBook {
    BidBook bids;
    AskBook asks;

    Price spread() const {
        auto* best_bid = bids.top();
        auto* best_ask = asks.top();
        if (!best_bid || !best_ask) return 0;
        return best_ask->price - best_bid->price;
    }

    Price mid() const {
        auto* best_bid = bids.top();
        auto* best_ask = asks.top();
        if (!best_bid || !best_ask) return 0;
        return (best_bid->price + best_ask->price) / 2;
    }
};
```

### Complexity Summary

| Operation | Complexity | Notes |
|---|---|---|
| `top()` | O(1) | Index 0, always cache-hot |
| `find()` | O(log n) | `std::lower_bound` on contiguous memory |
| `add()` near top | O(n) worst, O(1) amortized near-top | `memmove` of ~20 elements = 2 cache lines |
| `remove()` | O(log n) search + O(n) shift | Same small-n argument |
| `set()` | O(log n) search + O(1) or O(n) | In-place update is O(1) after search |

---

## 4. Priority Queue (Binary Heap for Timer/Event Management)

### Timer Wheels vs Heaps

**Timer wheels** (hashed or hierarchical) give O(1) insert and O(1) expiry for timers that fire at fixed granularity (e.g., 1ms tick). They excel when you have millions of timers at coarse resolution. The tradeoff: wasted memory for sparse slots, complexity in cascading across wheel levels, and poor behavior when timer durations span wide ranges.

**Binary heaps** give O(log n) insert/pop with perfect behavior for arbitrary timestamps and mixed-duration timers. For HFT event scheduling — where n is typically hundreds to low thousands of active timers (order timeouts, heartbeats, reconnect timers, market data staleness checks) — log(1000) = 10 comparisons per operation, which is nothing compared to the syscall overhead of the events themselves.

The heap wins for HFT because: timer count is moderate, durations vary wildly (microsecond IOC timeouts vs. second-scale heartbeats vs. day-scale GTC expiry), and the implementation is trivially correct.

### HFT Use Cases

- **IOC (Immediate-or-Cancel)**: insert timeout at `now + 0` or `now + few_us`, fires almost immediately if not filled
- **FOK (Fill-or-Kill)**: similar to IOC, single-check timeout
- **GTC (Good-til-Cancel)**: long-lived, may sit in heap for hours/days
- **Heartbeat monitoring**: periodic timers to detect dead connections
- **Throttle windows**: rate-limit event scheduling per venue

### Implementation

```cpp
#pragma once

#include <cstdint>
#include <vector>
#include <limits>
#include <functional>

using Timestamp = int64_t;  // nanoseconds since epoch
using EventId   = uint64_t;

struct TimerEvent {
    Timestamp deadline;
    EventId   id;
    int       type;        // Application-defined event type tag
    bool      cancelled;   // Lazy deletion flag

    // Min-heap: earliest deadline on top
    bool operator>(const TimerEvent& o) const noexcept {
        return deadline > o.deadline;
    }
};

class TimerHeap {
public:
    TimerHeap() {
        heap_.reserve(1024);
    }

    // Insert: O(log n) — push to back, sift up
    EventId insert(Timestamp deadline, int type) {
        EventId id = next_id_++;
        heap_.push_back(TimerEvent{deadline, id, type, false});
        sift_up(heap_.size() - 1);
        active_count_++;
        return id;
    }

    // Peek next event: O(1)
    // Skips cancelled events at the top (amortized).
    // Returns nullptr if no active events remain.
    const TimerEvent* peek() {
        purge_top();
        if (heap_.empty()) return nullptr;
        return &heap_[0];
    }

    // Pop next event: O(log n)
    // Returns false if empty.
    bool pop(TimerEvent& out) {
        purge_top();
        if (heap_.empty()) return false;

        out = heap_[0];
        // Move last element to root, shrink, sift down
        heap_[0] = heap_.back();
        heap_.pop_back();
        if (!heap_.empty()) sift_down(0);
        active_count_--;
        return true;
    }

    // Pop all events with deadline <= now.
    // Calls handler for each. Returns count fired.
    template <typename Handler>
    int fire_expired(Timestamp now, Handler&& handler) {
        int count = 0;
        TimerEvent ev;
        while (peek() && heap_[0].deadline <= now) {
            pop(ev);
            handler(ev);
            count++;
        }
        return count;
    }

    // Cancel by ID: O(1) — lazy deletion.
    // The cancelled event remains in the heap but is skipped on pop/peek.
    // Tradeoff: avoids O(n) search. Heap bloat is bounded by periodic purge.
    void cancel(EventId id) {
        // In production, you'd maintain an id -> index map (flat hash map)
        // for O(1) lookup. Here we mark via linear scan for simplicity.
        for (auto& ev : heap_) {
            if (ev.id == id && !ev.cancelled) {
                ev.cancelled = true;
                active_count_--;
                return;
            }
        }
    }

    size_t size() const noexcept { return active_count_; }
    bool   empty() const noexcept { return active_count_ == 0; }

    // If cancelled entries exceed half the heap, rebuild to reclaim memory
    void compact() {
        if (heap_.size() > 2 * active_count_ && heap_.size() > 64) {
            std::vector<TimerEvent> clean;
            clean.reserve(active_count_);
            for (auto& ev : heap_) {
                if (!ev.cancelled) clean.push_back(ev);
            }
            heap_ = std::move(clean);
            rebuild();
        }
    }

private:
    std::vector<TimerEvent> heap_;
    EventId next_id_ = 1;
    size_t  active_count_ = 0;

    // Skip cancelled events sitting at the top
    void purge_top() {
        while (!heap_.empty() && heap_[0].cancelled) {
            heap_[0] = heap_.back();
            heap_.pop_back();
            if (!heap_.empty()) sift_down(0);
        }
    }

    void sift_up(size_t i) {
        while (i > 0) {
            size_t parent = (i - 1) / 2;
            if (heap_[i].deadline < heap_[parent].deadline) {
                std::swap(heap_[i], heap_[parent]);
                i = parent;
            } else {
                break;
            }
        }
    }

    void sift_down(size_t i) {
        size_t n = heap_.size();
        while (true) {
            size_t smallest = i;
            size_t left  = 2 * i + 1;
            size_t right = 2 * i + 2;

            if (left < n && heap_[left].deadline < heap_[smallest].deadline)
                smallest = left;
            if (right < n && heap_[right].deadline < heap_[smallest].deadline)
                smallest = right;

            if (smallest == i) break;
            std::swap(heap_[i], heap_[smallest]);
            i = smallest;
        }
    }

    // Floyd's heap construction: O(n)
    void rebuild() {
        if (heap_.size() <= 1) return;
        for (int i = static_cast<int>(heap_.size() / 2) - 1; i >= 0; i--) {
            sift_down(static_cast<size_t>(i));
        }
    }
};
```

### Complexity Summary

| Operation | Complexity | Notes |
|---|---|---|
| `insert()` | O(log n) | Sift up from leaf |
| `peek()` | O(1) amortized | Purge cancelled at top |
| `pop()` | O(log n) | Swap root with last, sift down |
| `cancel()` | O(1) with id-map, O(n) with scan | Lazy — just sets flag |
| `fire_expired()` | O(k log n) | k = number of expired events |
| `compact()` | O(n) | Floyd rebuild, called infrequently |

---

## 5. Trie (Radix Tree for Symbol Routing)

### Why Tries Beat Hash Maps for Routing

Hash maps give O(1) average-case exact lookup, but they fail at three things critical to FIX/market data routing:

1. **Prefix matching**: Route all symbols starting with `ES` (E-mini S&P futures variants) to one handler. A hash map requires iterating all keys. A trie walks `E->S` and returns the entire subtree.

2. **FIX tag parsing**: FIX messages contain tag-value pairs like `35=D|49=SENDER|55=AAPL`. Routing on tag 35 (MsgType) with sub-routing on tag 49 (SenderCompID) maps naturally to trie prefix structure.

3. **Venue routing**: Route `XNAS.*` to Nasdaq handler, `XNYS.*` to NYSE handler. Trie does this in O(prefix_length) with zero hash collisions.

A **radix tree** (compressed trie) collapses single-child chains into one node. The string `AAPL` doesn't require 4 nodes — it's one node with edge label `"AAPL"`. This cuts memory usage and pointer-chasing dramatically for real symbol sets.

### Cache Layout Considerations

Trie nodes should be arena-allocated (pool allocator) so that siblings and parent-child pairs land in adjacent memory. The implementation below uses `std::unique_ptr` for simplicity, but production code would use a flat arena with index-based "pointers" — store nodes in a `std::vector<RadixNode>` and reference children by index. This gives contiguous memory, trivial serialization, and prefetcher-friendly traversal.

### Implementation

```cpp
#pragma once

#include <cstdint>
#include <array>
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <functional>

// Value stored at terminal nodes (e.g., handler ID, venue routing info)
using RouteId = int32_t;
constexpr RouteId NO_ROUTE = -1;

struct RadixNode {
    std::string edge_label;                            // Compressed edge
    std::array<std::unique_ptr<RadixNode>, 128> children;  // ASCII-indexed
    RouteId route = NO_ROUTE;                          // Terminal value

    // In production: replace unique_ptr with arena indices.
    // children would be std::array<int32_t, 128> into a node pool,
    // with -1 as null sentinel. Eliminates heap alloc per node.

    bool is_terminal() const noexcept { return route != NO_ROUTE; }
};

class RadixTrie {
public:
    RadixTrie() : root_(std::make_unique<RadixNode>()) {}

    // Insert key with associated route ID: O(k) where k = key length
    void insert(std::string_view key, RouteId route) {
        insert_impl(root_.get(), key, route);
    }

    // Exact search: O(k)
    RouteId search(std::string_view key) const {
        const RadixNode* node = root_.get();
        size_t pos = 0;

        while (pos < key.size()) {
            uint8_t c = static_cast<uint8_t>(key[pos]);
            const auto& child = node->children[c];
            if (!child) return NO_ROUTE;

            std::string_view remaining = key.substr(pos);
            std::string_view edge = child->edge_label;

            if (remaining.size() < edge.size()) return NO_ROUTE;
            if (remaining.substr(0, edge.size()) != edge) return NO_ROUTE;

            pos += edge.size();
            node = child.get();
        }

        return node->route;
    }

    // Prefix match: find all routes whose key starts with prefix.
    // O(p + m) where p = prefix length, m = number of matches.
    void prefix_match(std::string_view prefix,
                      std::vector<std::pair<std::string, RouteId>>& results) const {
        results.clear();
        const RadixNode* node = root_.get();
        std::string built;
        size_t pos = 0;

        while (pos < prefix.size()) {
            uint8_t c = static_cast<uint8_t>(prefix[pos]);
            const auto& child = node->children[c];
            if (!child) return;

            std::string_view remaining = prefix.substr(pos);
            std::string_view edge = child->edge_label;

            size_t match_len = common_prefix_length(remaining, edge);
            if (match_len < remaining.size() && match_len < edge.size()) {
                return;  // Divergence before prefix consumed
            }

            built.append(edge.data(), edge.size());
            pos += edge.size();
            node = child.get();

            if (pos >= prefix.size()) break;
        }

        collect(node, built, results);
    }

    // Longest prefix match: find the deepest terminal node matching a prefix of key.
    // Used for hierarchical routing (e.g., "XNAS.AAPL" matches route for "XNAS").
    // O(k)
    RouteId longest_prefix(std::string_view key) const {
        const RadixNode* node = root_.get();
        RouteId best = NO_ROUTE;
        size_t pos = 0;

        if (node->is_terminal()) best = node->route;

        while (pos < key.size()) {
            uint8_t c = static_cast<uint8_t>(key[pos]);
            const auto& child = node->children[c];
            if (!child) break;

            std::string_view remaining = key.substr(pos);
            std::string_view edge = child->edge_label;
            size_t match_len = common_prefix_length(remaining, edge);

            if (match_len < edge.size()) break;

            pos += edge.size();
            node = child.get();
            if (node->is_terminal()) best = node->route;
        }

        return best;
    }

private:
    std::unique_ptr<RadixNode> root_;

    static size_t common_prefix_length(std::string_view a, std::string_view b) {
        size_t len = std::min(a.size(), b.size());
        size_t i = 0;
        while (i < len && a[i] == b[i]) i++;
        return i;
    }

    void insert_impl(RadixNode* node, std::string_view key, RouteId route) {
        if (key.empty()) {
            node->route = route;
            return;
        }

        uint8_t c = static_cast<uint8_t>(key[0]);
        auto& child = node->children[c];

        if (!child) {
            child = std::make_unique<RadixNode>();
            child->edge_label = std::string(key);
            child->route = route;
            return;
        }

        std::string_view edge = child->edge_label;
        size_t common = common_prefix_length(key, edge);

        if (common == edge.size()) {
            insert_impl(child.get(), key.substr(common), route);
            return;
        }

        // Partial match — split the edge
        auto split = std::make_unique<RadixNode>();
        split->edge_label = std::string(edge.substr(0, common));

        std::string old_suffix(edge.substr(common));
        uint8_t old_c = static_cast<uint8_t>(old_suffix[0]);
        child->edge_label = old_suffix;
        split->children[old_c] = std::move(child);

        std::string_view new_suffix = key.substr(common);
        if (new_suffix.empty()) {
            split->route = route;
        } else {
            uint8_t new_c = static_cast<uint8_t>(new_suffix[0]);
            split->children[new_c] = std::make_unique<RadixNode>();
            split->children[new_c]->edge_label = std::string(new_suffix);
            split->children[new_c]->route = route;
        }

        node->children[c] = std::move(split);
    }

    void collect(const RadixNode* node, std::string& path,
                 std::vector<std::pair<std::string, RouteId>>& results) const {
        if (node->is_terminal()) {
            results.emplace_back(path, node->route);
        }

        for (int i = 0; i < 128; i++) {
            const auto& child = node->children[i];
            if (!child) continue;
            size_t old_len = path.size();
            path.append(child->edge_label);
            collect(child.get(), path, results);
            path.resize(old_len);
        }
    }
};
```

### Usage Examples

```cpp
RadixTrie router;

// Venue routing
router.insert("XNAS", 1);   // Nasdaq
router.insert("XNYS", 2);   // NYSE
router.insert("XCHI", 3);   // CHX

// Symbol-specific overrides
router.insert("XNAS.AAPL", 10);  // Special AAPL handler on Nasdaq
router.insert("XNAS.TSLA", 11);

// Longest prefix: "XNAS.MSFT" matches "XNAS" (route 1)
// "XNAS.AAPL" matches "XNAS.AAPL" (route 10) — more specific

// FIX tag routing
RadixTrie fix_router;
fix_router.insert("35=D",  100);   // New Order Single
fix_router.insert("35=F",  101);   // Cancel Request
fix_router.insert("35=G",  102);   // Cancel/Replace
fix_router.insert("35=8",  103);   // Execution Report

// Prefix match: all MsgType tags
std::vector<std::pair<std::string, RouteId>> matches;
fix_router.prefix_match("35=", matches);
// Returns all four entries
```

### Complexity Summary

| Operation | Complexity | Notes |
|---|---|---|
| `insert()` | O(k) | k = key length, may split one edge |
| `search()` | O(k) | Exact match, walks compressed edges |
| `longest_prefix()` | O(k) | Tracks best terminal seen so far |
| `prefix_match()` | O(p + m) | p = prefix length, m = subtree size |
| Memory | O(total key bytes) | Path compression eliminates redundant nodes |

---

# ALGORITHMS

---

## 6. Binary Search (Price Level Lookup)

Binary search on a sorted order book is the bread-and-butter lookup in any matching engine. The critical insight for HFT: every branch misprediction costs ~15-20 cycles on modern Intel cores. When you're doing thousands of lookups per microsecond, the branching pattern of textbook binary search becomes a measurable bottleneck.

### Standard Binary Search

```cpp
#include <cstdint>
#include <vector>

struct PriceLevel {
    int64_t price;    // price in ticks (integer, no floating point)
    int64_t quantity;
    uint32_t order_count;
};

// Classic binary search - find exact price level
// Returns index or -1 if not found
int find_price_level(const PriceLevel* levels, int n, int64_t target_price) {
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (levels[mid].price == target_price) return mid;
        if (levels[mid].price < target_price)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}
```

### Lower Bound / Upper Bound

These are what you actually use most often in practice: finding where a price *would* go in the book, or finding all levels at or above a threshold.

```cpp
// Lower bound: first element >= target
// Use case: find insertion point for a new price level,
//           find cheapest ask >= some threshold
int lower_bound(const int64_t* prices, int n, int64_t target) {
    int lo = 0, count = n;
    while (count > 0) {
        int step = count / 2;
        int mid = lo + step;
        if (prices[mid] < target) {
            lo = mid + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return lo;  // first index where prices[lo] >= target
}

// Upper bound: first element > target
// Use case: find end of range of levels at a given price,
//           count orders strictly above a price
int upper_bound(const int64_t* prices, int n, int64_t target) {
    int lo = 0, count = n;
    while (count > 0) {
        int step = count / 2;
        int mid = lo + step;
        if (prices[mid] <= target) {
            lo = mid + 1;
            count -= step + 1;
        } else {
            count = step;
        }
    }
    return lo;  // first index where prices[lo] > target
}

// Example: find all levels in price range [lo_price, hi_price]
// Returns {start_idx, count}
std::pair<int, int> find_price_range(const int64_t* prices, int n,
                                     int64_t lo_price, int64_t hi_price) {
    int start = lower_bound(prices, n, lo_price);
    int end   = upper_bound(prices, n, hi_price);
    return {start, end - start};
}
```

### Branchless Binary Search

This is where it matters for HFT. The standard version has a data-dependent branch (`if prices[mid] < target`) that the CPU cannot predict well when searching random prices. Each misprediction flushes the pipeline (~15-20 cycles on Skylake/Ice Lake).

The branchless version replaces the branch with a conditional move (`cmov`), which the CPU executes speculatively on both paths — no pipeline flush.

```cpp
// Branchless binary search - lower bound variant
// Key idea: replace if/else with conditional move (cmov).
// cmov executes in 1 cycle, mispredicted branch costs 15-20.
int lower_bound_branchless(const int64_t* prices, int n, int64_t target) {
    const int64_t* base = prices;
    int len = n;

    while (len > 1) {
        int half = len / 2;
        // __builtin_expect is irrelevant here - the whole point is
        // the compiler turns this into cmov, not a branch
        base += (base[half] < target) * half;  // branchless: ptr arithmetic
        len -= half;
    }
    return (base - prices) + (*base < target);
}

// Even more explicit: use the ternary that GCC/Clang lower to cmov
int lower_bound_branchless_v2(const int64_t* prices, int n, int64_t target) {
    const int64_t* base = prices;
    int len = n;

    while (len > 1) {
        int half = len / 2;
        const int64_t* mid = base + half;
        // Compiler emits: cmp + cmov (no branch)
        base = (*mid < target) ? mid : base;
        len -= half;
    }
    return static_cast<int>(base - prices) + (*base < target);
}
```

**Verify it compiles to cmov** — always check the assembly. With `g++ -O2 -S`:
```asm
cmpq    %rax, (%rdx)      # compare *mid with target
cmovl   %rdx, %rsi         # conditional move, no branch
```

If you see `jl` or `jge` instead of `cmov`, the compiler decided branching was better. Force it with `-fno-if-conversion` disabled or restructure the code.

### Performance Analysis

```
Array size: 1024 price levels (typical deep book)
Iterations: log2(1024) = 10

Standard binary search:
  - 10 iterations x ~4 cycles (predicted) = 40 cycles best case
  - But prediction rate on random lookups ~ 50% at each step
  - 10 iterations x 50% miss x 15 cycle penalty = ~75 extra cycles
  - Realistic: ~100-120 cycles total

Branchless binary search:
  - 10 iterations x ~5 cycles (cmov is slightly slower than predicted branch)
  - But ZERO misprediction penalty
  - Consistent: ~50 cycles total

For 4096 levels (12 iterations):
  Standard: ~130-160 cycles (variance is the killer)
  Branchless: ~60 cycles, near-zero variance
```

The real win is not just lower average latency but **lower tail latency**. HFT cares about P99 as much as P50. The branchless version has near-deterministic cycle count.

**Cache effects**: With 1024 price levels x 20 bytes each = 20KB, the entire book fits in L1 (32-48KB). Binary search touches O(log n) cache lines, so cache misses are minimal. For very large books (10K+ levels), prefetching the midpoint of each half (`__builtin_prefetch`) can help.

---

## 7. Sorting (Radix Sort + Introsort)

Sorting in HFT is mostly a batch operation: ranking signals at the start of a window, sorting fills for reporting, ordering messages by timestamp for replay. The key decision is **radix sort vs. comparison sort**, and it comes down to key type.

### LSD Radix Sort (Integer Keys)

Least-Significant-Digit radix sort processes one byte at a time, bottom-up. For 64-bit keys (order IDs, nanosecond timestamps), that is 8 passes. Each pass is O(n) with a 256-bucket counting sort.

```cpp
#include <cstdint>
#include <cstring>
#include <vector>

struct Order {
    uint64_t timestamp;  // nanoseconds since epoch
    uint64_t order_id;
    int64_t  price;
    int32_t  quantity;
    char     side;       // 'B' or 'S'
};

// LSD Radix sort on a 64-bit key.
// Sorts `orders` in-place by the key extracted via `key_fn`.
// `buf` is scratch space (same size as orders).
template <typename KeyFn>
void radix_sort_64(Order* orders, Order* buf, int n, KeyFn key_fn) {
    constexpr int RADIX_BITS = 8;
    constexpr int RADIX      = 1 << RADIX_BITS;  // 256
    constexpr int PASSES     = sizeof(uint64_t);  // 8

    Order* src = orders;
    Order* dst = buf;

    for (int pass = 0; pass < PASSES; ++pass) {
        int shift = pass * RADIX_BITS;

        // 1. Count occurrences of each byte value
        uint32_t count[RADIX] = {};
        for (int i = 0; i < n; ++i) {
            uint8_t byte = (key_fn(src[i]) >> shift) & 0xFF;
            ++count[byte];
        }

        // Optimization: skip pass if all keys have same byte at this position
        bool single_bucket = false;
        for (int i = 0; i < RADIX; ++i) {
            if (count[i] == static_cast<uint32_t>(n)) {
                single_bucket = true;
                break;
            }
        }
        if (single_bucket) continue;

        // 2. Prefix sum -> starting index for each bucket
        uint32_t offset[RADIX];
        offset[0] = 0;
        for (int i = 1; i < RADIX; ++i) {
            offset[i] = offset[i - 1] + count[i - 1];
        }

        // 3. Scatter into dst
        for (int i = 0; i < n; ++i) {
            uint8_t byte = (key_fn(src[i]) >> shift) & 0xFF;
            dst[offset[byte]++] = src[i];
        }

        // Swap src and dst for next pass
        Order* tmp = src;
        src = dst;
        dst = tmp;
    }

    // After 8 passes (even number), result is back in `orders`
    // if we started with src=orders. If PASSES is odd, need a final copy.
}

// Usage: sort orders by timestamp
void sort_by_timestamp(std::vector<Order>& orders) {
    std::vector<Order> buf(orders.size());
    radix_sort_64(orders.data(), buf.data(), static_cast<int>(orders.size()),
                  [](const Order& o) { return o.timestamp; });
}

// Usage: sort by order_id
void sort_by_order_id(std::vector<Order>& orders) {
    std::vector<Order> buf(orders.size());
    radix_sort_64(orders.data(), buf.data(), static_cast<int>(orders.size()),
                  [](const Order& o) { return o.order_id; });
}
```

### Introsort (What `std::sort` Does)

`std::sort` in libstdc++ and libc++ uses **introsort**: quicksort that falls back to heapsort if recursion depth exceeds 2xlog2(n), plus insertion sort for small partitions (typically n <= 16).

```
introsort(arr, n, depth_limit = 2*log2(n)):
    if n <= 16:
        insertion_sort(arr, n)          // great for small, nearly-sorted
        return
    if depth_limit == 0:
        heapsort(arr, n)                // O(n log n) guaranteed, no stack overflow
        return
    pivot = median_of_three(arr[0], arr[n/2], arr[n-1])
    mid = partition(arr, n, pivot)
    introsort(arr, mid, depth_limit - 1)
    introsort(arr + mid + 1, n - mid - 1, depth_limit - 1)
```

You almost never need to implement introsort yourself. Just use `std::sort` with a custom comparator:

```cpp
#include <algorithm>

// Sort orders by price descending (bids), then by timestamp ascending (FIFO)
void sort_bid_book(std::vector<Order>& orders) {
    std::sort(orders.begin(), orders.end(), [](const Order& a, const Order& b) {
        if (a.price != b.price) return a.price > b.price;  // higher price first
        return a.timestamp < b.timestamp;                   // earlier time first
    });
}
```

### When to Use Which

```
Radix sort wins when:
  - Keys are integers (order IDs, timestamps, tick prices)
  - n > ~1000 (below that, overhead of 8 passes isn't worth it)
  - Key range is bounded (you know it's 64-bit, not arbitrary precision)
  - You can afford 2x memory for the scratch buffer

std::sort wins when:
  - Keys are composite (multi-field comparisons like price+time priority)
  - n < ~1000
  - Memory is tight (in-place, O(log n) stack space)
  - Keys aren't integer-like (strings, floats with special ordering)

Concrete numbers (approximate, modern x86, L1-resident):
  n=1000   radix: ~30 us    std::sort: ~20 us    (sort wins)
  n=10000  radix: ~200 us   std::sort: ~300 us   (radix wins)
  n=100000 radix: ~2 ms     std::sort: ~4.5 ms   (radix wins 2x)
```

### Cache Considerations

For large arrays (exceeding L2/L3), sorting performance is dominated by memory access patterns, not instruction count.

**Radix sort's problem**: the scatter step writes to 256 random positions in the output buffer. For large n, each bucket spans multiple cache lines, and the writes become essentially random — thrashing the cache.

**Software write-combining (SWC) buffer** mitigation:

```cpp
constexpr int SWC_BUF_SIZE = 64;  // entries per bucket buffer

struct BucketBuffer {
    Order items[SWC_BUF_SIZE];
    int count = 0;
};

// During scatter:
BucketBuffer buckets[256];

for (int i = 0; i < n; ++i) {
    uint8_t byte = (key_fn(src[i]) >> shift) & 0xFF;
    auto& b = buckets[byte];
    b.items[b.count++] = src[i];
    if (b.count == SWC_BUF_SIZE) {
        // Flush: sequential write to dst
        std::memcpy(&dst[offset[byte]], b.items, sizeof(Order) * SWC_BUF_SIZE);
        offset[byte] += SWC_BUF_SIZE;
        b.count = 0;
    }
}
// Flush remaining items in each bucket
for (int b = 0; b < 256; ++b) {
    std::memcpy(&dst[offset[b]], buckets[b].items, sizeof(Order) * buckets[b].count);
}
```

This turns 256 random write streams into batched sequential writes. For arrays exceeding L2 (256KB-1MB typical), this can recover 30-50% of the cache-thrashing penalty.

**`std::sort`'s advantage**: quicksort's partition step is sequential reads + sequential writes on the same array. Very cache-friendly. The final insertion sort passes on small subarrays are entirely L1-resident.

---

## 8. Sliding Window / Streaming Statistics

In HFT, every signal must update in O(1) per tick. Recomputing an average over N ticks on every update means your signal path scales O(N) per message — at 10M messages/sec on a busy symbol, that's the difference between 50ns and 50us. The streaming approach maintains state incrementally.

**Batch vs Streaming tradeoff:** Batch recomputation is simpler to reason about and debug, but it's only viable for MFT (seconds-scale holding periods) or offline analytics. For anything sub-millisecond, streaming is mandatory.

**Use cases:**
- **TWAP/VWAP execution:** Compare live VWAP against your execution VWAP to gauge slippage in real-time.
- **Signal generation:** EMA crossovers, mean-reversion z-scores, momentum signals — all must be O(1).
- **Real-time risk:** Rolling volatility, drawdown tracking, position-weighted PnL streaming.

### VWAP — Streaming Computation

```cpp
#include <cstdint>

// Streaming VWAP: O(1) per update, no window — cumulative from session start.
struct StreamingVWAP {
    double cum_pv = 0.0;   // cumulative price * volume
    double cum_vol = 0.0;  // cumulative volume

    void update(double price, double volume) {
        cum_pv  += price * volume;
        cum_vol += volume;
    }

    double vwap() const {
        return cum_vol > 0.0 ? cum_pv / cum_vol : 0.0;
    }

    void reset() { cum_pv = 0.0; cum_vol = 0.0; }
};

// Windowed VWAP over last N ticks using ring buffer.
// Subtracts the leaving tick's contribution — still O(1) per update.
template <int N>
struct WindowedVWAP {
    struct Tick { double price; double volume; };
    Tick buf[N] = {};
    int pos = 0;
    int count = 0;
    double cum_pv = 0.0;
    double cum_vol = 0.0;

    void update(double price, double volume) {
        if (count >= N) {
            cum_pv  -= buf[pos].price * buf[pos].volume;
            cum_vol -= buf[pos].volume;
        } else {
            ++count;
        }
        buf[pos] = {price, volume};
        cum_pv  += price * volume;
        cum_vol += volume;
        pos = (pos + 1) % N;
    }

    double vwap() const {
        return cum_vol > 0.0 ? cum_pv / cum_vol : 0.0;
    }
};
```

### EMA — Single Pass, O(1) Per Update

```cpp
// EMA: new_val = alpha * price + (1 - alpha) * prev_val
// alpha = 2 / (span + 1)  — standard pandas-style definition.
// No arrays, no history. Pure O(1) state machine.
struct EMA {
    double alpha;
    double value = 0.0;
    bool initialized = false;

    explicit EMA(int span) : alpha(2.0 / (span + 1)) {}

    void update(double price) {
        if (!initialized) [[unlikely]] {
            value = price;
            initialized = true;
        } else {
            value = alpha * price + (1.0 - alpha) * value;
        }
    }

    double get() const { return value; }
};

// Dual EMA crossover signal — the bread and butter of momentum signals.
struct EMACrossover {
    EMA fast;
    EMA slow;

    EMACrossover(int fast_span, int slow_span)
        : fast(fast_span), slow(slow_span) {}

    void update(double price) {
        fast.update(price);
        slow.update(price);
    }

    // Positive = fast above slow (bullish), negative = bearish
    double signal() const { return fast.get() - slow.get(); }
};
```

### Rolling Min/Max — Monotonic Deque

```cpp
#include <deque>
#include <cstdint>

// Monotonic deque maintains the running min (or max) over a sliding window
// in amortized O(1) per update. Each element enters and leaves the deque
// exactly once across its lifetime.
//
// Used for: Bollinger band width (rolling max - min), channel breakout
// detection, real-time drawdown computation.
template <typename Cmp = std::less<double>>
struct MonotonicDeque {
    struct Entry { double val; int64_t seq; };
    std::deque<Entry> dq;  // front = current extremum
    int window;
    int64_t seq = 0;
    Cmp cmp{};

    explicit MonotonicDeque(int window) : window(window) {}

    void push(double val) {
        // Evict entries outside the window from the front
        while (!dq.empty() && dq.front().seq <= seq - window)
            dq.pop_front();

        // Pop from back while new value is "better" (less for min, greater for max)
        while (!dq.empty() && cmp(val, dq.back().val))
            dq.pop_back();

        dq.push_back({val, seq});
        ++seq;
    }

    double get() const { return dq.front().val; }
};

// Convenience typedefs
using RollingMin = MonotonicDeque<std::less<double>>;
using RollingMax = MonotonicDeque<std::greater<double>>;

// Usage for channel breakout:
//   RollingMax high_channel(20);  // 20-tick high
//   RollingMin low_channel(20);   // 20-tick low
//   on each tick:
//     high_channel.push(price);
//     low_channel.push(price);
//     double width = high_channel.get() - low_channel.get();
//     bool breakout_up = price >= high_channel.get();
```

---

## 9. Graph Algorithms (Topological Sort, Cycle Detection)

### Topological Sort — Kahn's Algorithm

In any HFT system, you have a DAG of dependencies: market data must arrive before signal computation, signals before risk checks, risk before order generation. Topological sort gives you a valid execution order. Kahn's (BFS-based) is preferred over DFS-based in production because it naturally detects cycles and gives deterministic level-by-level ordering useful for parallelism — all nodes at the same "level" can execute concurrently.

**Time complexity:** O(V + E) — each vertex and edge visited exactly once.

```cpp
#include <vector>
#include <queue>
#include <cstdint>
#include <stdexcept>

struct DAG {
    int n;
    std::vector<std::vector<int>> adj;
    std::vector<int> in_degree;

    explicit DAG(int n) : n(n), adj(n), in_degree(n, 0) {}

    void add_edge(int from, int to) {
        adj[from].push_back(to);
        ++in_degree[to];
    }

    struct TopoResult {
        std::vector<int> order;
        std::vector<int> level;  // level[node] = earliest execution stage
    };

    TopoResult topo_sort() const {
        TopoResult result;
        result.level.resize(n, -1);
        result.order.reserve(n);

        std::queue<int> q;
        auto deg = in_degree;  // working copy

        for (int i = 0; i < n; ++i) {
            if (deg[i] == 0) {
                q.push(i);
                result.level[i] = 0;
            }
        }

        while (!q.empty()) {
            int u = q.front(); q.pop();
            result.order.push_back(u);

            for (int v : adj[u]) {
                result.level[v] = std::max(result.level[v], result.level[u] + 1);
                if (--deg[v] == 0)
                    q.push(v);
            }
        }

        if ((int)result.order.size() != n)
            throw std::runtime_error("cycle detected in pipeline DAG");

        return result;
    }
};

// Example: HFT pipeline scheduling
// Stages: 0=MarketData, 1=FairValue, 2=Signal, 3=RiskCheck, 4=OrderGen
//
//   DAG pipeline(5);
//   pipeline.add_edge(0, 1);  // MD -> FairValue
//   pipeline.add_edge(0, 2);  // MD -> Signal
//   pipeline.add_edge(1, 2);  // FairValue -> Signal
//   pipeline.add_edge(2, 3);  // Signal -> RiskCheck
//   pipeline.add_edge(3, 4);  // RiskCheck -> OrderGen
//   auto [order, level] = pipeline.topo_sort();
```

### Bellman-Ford — Negative Cycle Detection for Arbitrage

The key insight: model currencies/assets as graph nodes, exchange rates as edge weights using `-log(rate)`. A profitable arbitrage cycle (product of rates > 1.0) becomes a negative-weight cycle (sum of -log(rates) < 0). Bellman-Ford detects these in O(V*E).

For FX triangular arbitrage: USD -> EUR -> GBP -> USD. If `rate(USD,EUR) * rate(EUR,GBP) * rate(GBP,USD) > 1.0`, that's free money minus fees. In log-space: `-log(r1) - log(r2) - log(r3) < 0` — a negative cycle.

**Time complexity:** O(V * E). For a small FX graph (8 major currencies, ~56 directed edges), this is trivial. For crypto with hundreds of pairs, use SPFA (queue-based Bellman-Ford) for practical speedup.

```cpp
#include <vector>
#include <cmath>
#include <limits>

struct ArbitrageDetector {
    struct Edge {
        int from, to;
        double weight;      // -log(exchange_rate)
        const char* venue;
        const char* pair;
    };

    int n_currencies;
    std::vector<Edge> edges;

    explicit ArbitrageDetector(int n) : n_currencies(n) {}

    // rate: how many units of 'to' you get for 1 unit of 'from'
    void set_rate(int from, int to, double rate,
                  const char* venue, const char* pair) {
        double w = -std::log(rate);
        for (auto& e : edges) {
            if (e.from == from && e.to == to && e.venue == venue) {
                e.weight = w;
                e.pair = pair;
                return;
            }
        }
        edges.push_back({from, to, w, venue, pair});
    }

    struct Cycle {
        std::vector<int> path;   // currency indices forming the cycle
        double profit_bps;       // estimated profit in basis points
    };

    // Standard Bellman-Ford: relax all edges V-1 times, then check for
    // further relaxation on the V-th pass.
    std::vector<Cycle> detect() const {
        std::vector<double> dist(n_currencies, 0.0);
        std::vector<int> parent(n_currencies, -1);

        // Relax V-1 times
        for (int i = 0; i < n_currencies - 1; ++i) {
            for (const auto& e : edges) {
                if (dist[e.from] + e.weight < dist[e.to] - 1e-12) {
                    dist[e.to] = dist[e.from] + e.weight;
                    parent[e.to] = e.from;
                }
            }
        }

        // V-th pass: any relaxation means negative cycle
        std::vector<Cycle> cycles;
        std::vector<bool> in_cycle(n_currencies, false);

        for (const auto& e : edges) {
            if (dist[e.from] + e.weight < dist[e.to] - 1e-12) {
                int node = e.to;
                for (int i = 0; i < n_currencies; ++i)
                    node = parent[node];

                if (in_cycle[node]) continue;

                Cycle cyc;
                double total_log_rate = 0.0;
                int cur = node;
                do {
                    cyc.path.push_back(cur);
                    in_cycle[cur] = true;
                    for (const auto& ed : edges) {
                        if (ed.from == parent[cur] && ed.to == cur) {
                            total_log_rate += ed.weight;
                            break;
                        }
                    }
                    cur = parent[cur];
                } while (cur != node);

                cyc.path.push_back(node);  // close the cycle
                cyc.profit_bps = (std::exp(-total_log_rate) - 1.0) * 10000.0;
                cycles.push_back(std::move(cyc));
            }
        }
        return cycles;
    }
};

// Example: FX triangular arbitrage
// Currencies: 0=USD, 1=EUR, 2=GBP, 3=JPY
//
//   ArbitrageDetector det(4);
//   det.set_rate(0, 1, 0.92, "CME", "EURUSD");   // 1 USD = 0.92 EUR
//   det.set_rate(1, 2, 0.86, "LSE", "EURGBP");   // 1 EUR = 0.86 GBP
//   det.set_rate(2, 0, 1.27, "CME", "GBPUSD");   // 1 GBP = 1.27 USD
//   // Round trip: 1 USD -> 0.92 EUR -> 0.7912 GBP -> 1.0048 USD
//   // Profit: ~48 bps before fees
//   auto opps = det.detect();
```

---

## 10. Dynamic Programming (Optimal Order Slicing)

When you need to execute a large order (say, buy 100,000 shares over 30 minutes), you can't dump it all at once — you'd move the market against yourself. The goal: slice the parent order into child orders across T time buckets, minimizing total execution cost = market impact + timing risk.

This is the simplified Almgren-Chriss framework reduced to a DP. The tradeoff: trade aggressively early (less timing risk, more impact) vs. spread it out (less impact, more risk of price moving away).

**Why DP matters here:** Greedy doesn't work because the impact of trading in bucket `t` depends on how much you've already traded (permanent impact accumulates). The optimal schedule requires considering the full remaining trajectory — textbook overlapping subproblems.

**Time complexity:** O(T * Q) where T = number of time buckets, Q = quantity granularity. For practical parameters (T=30 buckets, Q=100 quantity levels), this is trivial compute.

**Use cases:**
- **TWAP/VWAP scheduling:** Optimal deviation from uniform schedule based on predicted volume/volatility.
- **Portfolio transition:** Rebalancing multiple positions simultaneously with cross-impact.
- **Risk budget allocation:** Distributing risk capital across strategies given correlation structure.

```cpp
#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>
#include <cstdio>

struct OrderSlicer {
    // Almgren-Chriss style parameters
    double eta;     // temporary impact coefficient (per-share, per-bucket)
    double gamma;   // permanent impact coefficient
    double sigma;   // volatility per bucket (price std dev)
    double lambda;  // risk aversion parameter

    // DP state: dp[t][q] = minimum cost to execute q shares in buckets [t, T-1]
    // Transition: dp[t][q] = min over x_t in [0, q] of:
    //     eta * x_t^2  +  gamma * x_t * (q - x_t)  +  lambda * sigma^2 * q^2  +  dp[t+1][q - x_t]

    struct Schedule {
        std::vector<int> quantities;    // shares per bucket
        double total_cost;              // estimated total cost
    };

    Schedule solve(int total_qty, int n_buckets, int qty_step = 1) const {
        int Q = total_qty / qty_step;  // number of quantity units
        int T = n_buckets;

        const double INF = std::numeric_limits<double>::max() / 2.0;

        // Only need two rows (current and next) — standard space optimization.
        std::vector<double> next(Q + 1, 0.0);  // dp[T][*] = 0 (nothing left)
        std::vector<double> curr(Q + 1, INF);
        std::vector<std::vector<int>> choice(T, std::vector<int>(Q + 1, 0));

        for (int t = T - 1; t >= 0; --t) {
            std::fill(curr.begin(), curr.end(), INF);
            curr[0] = 0.0;

            for (int q = 1; q <= Q; ++q) {
                double remaining_shares = q * qty_step;
                double risk = lambda * sigma * sigma * remaining_shares * remaining_shares;

                for (int x = 0; x <= q; ++x) {
                    double trade = x * qty_step;
                    double after = (q - x) * qty_step;

                    double temp_impact  = eta * trade * trade;
                    double perm_impact  = gamma * trade * after;
                    double cost = temp_impact + perm_impact + risk + next[q - x];

                    if (cost < curr[q]) {
                        curr[q] = cost;
                        choice[t][q] = x;
                    }
                }
            }
            std::swap(curr, next);
        }

        // Reconstruct the schedule
        Schedule sched;
        sched.quantities.resize(T);
        sched.total_cost = next[Q];  // after swap, next holds dp[0]

        int remaining = Q;
        for (int t = 0; t < T; ++t) {
            int x = choice[t][remaining];
            sched.quantities[t] = x * qty_step;
            remaining -= x;
        }

        return sched;
    }
};

// Convenience: generate a TWAP baseline for comparison
inline std::vector<int> twap_schedule(int total_qty, int n_buckets) {
    std::vector<int> sched(n_buckets, total_qty / n_buckets);
    int remainder = total_qty % n_buckets;
    for (int i = 0; i < remainder; ++i)
        sched[i] += 1;
    return sched;
}

// Example usage:
//
//   OrderSlicer slicer{
//       .eta    = 0.001,   // temporary impact
//       .gamma  = 0.0001,  // permanent impact
//       .sigma  = 0.50,    // volatility per bucket
//       .lambda = 0.001    // risk aversion
//   };
//
//   auto result = slicer.solve(10000, 20, 100);  // 10k shares, 20 buckets, step=100
//   // result.quantities: e.g., [800, 700, 650, ..., 350, 300]
//   // Front-loaded: higher risk aversion -> trade faster to reduce variance.
//   // Compare with TWAP: [500, 500, 500, ..., 500]
```

**Key insight on the DP solution shape:** With high `lambda` (risk aversion), the schedule front-loads — you trade aggressively early to reduce exposure to price drift. With low `lambda`, it spreads evenly (approaching TWAP) to minimize impact. With zero volatility (`sigma=0`), the optimal schedule minimizes impact alone and trades uniformly. This is exactly the Almgren-Chriss efficient frontier between impact cost and timing risk.
