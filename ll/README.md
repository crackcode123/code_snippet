# cpu_low_latency_runtime

Header-only C++17 building blocks for low-latency, concurrent systems.

## Components

| Header | Description |
|---|---|
| `mutex_ring_buffer.hpp` | Thread-safe bounded MPMC ring buffer (mutex + condition variables) |
| `lockfree_spsc.hpp` | Lock-free bounded SPSC ring buffer (acquire/release atomics, cache-line padded) |
| `ring_buffer_lockfree_CAS_MPMC.hpp` | Lock-free bounded MPMC ring buffer (CAS loops, per-slot sequence numbers) |
| `seqslot_MPMC.hpp` | Lock-free bounded MPMC ring buffer (Vyukov-style fetch_add + per-slot spin) |

## mutex_ring_buffer.hpp

Bounded ring buffer supporting multiple producers and multiple consumers.

- **Blocking**: `push()` blocks when full, `pop()` blocks when empty
- **Non-blocking**: `try_push()` / `try_pop()` return immediately
- **Timed**: `try_push_for()` / `try_pop_for()` accept a `std::chrono::duration` timeout
- Compile-time capacity (no heap allocation on the hot path)
- Uninitialized storage — `T` does not need to be default-constructible
- Lock released before `notify_one` to avoid waking a thread that immediately re-blocks

### Usage

```cpp
#include "mutex_ring_buffer.hpp"

low_latency::MutexRingBuffer<int, 1024> queue;

// Producer (blocks if full)
queue.push(42);

// Consumer (blocks if empty)
int val = queue.pop();

// Non-blocking
if (queue.try_push(99)) { /* success */ }
auto maybe = queue.try_pop();  // std::optional<int>

// Timed
using namespace std::chrono_literals;
queue.try_push_for(100ms, 7);
auto result = queue.try_pop_for(50us);
```

## lockfree_spsc.hpp

Lock-free bounded ring buffer for exactly one producer thread and one consumer thread.

- **Zero locks, zero syscalls** on the data path
- `head_` (producer) and `tail_` (consumer) on **separate cache lines** — no false sharing
- **Acquire/release** memory ordering — correct on ARM (LDAR/STLR), free on x86 (plain MOV)
- Capacity rounded to next power of 2 — wrap via bitmask, no modulo
- Uninitialized storage — `T` does not need to be default-constructible
- `push()` / `pop()` spin-wait; `try_push()` / `try_pop()` return immediately

### Usage

```cpp
#include "lockfree_spsc.hpp"

low_latency::LockfreeSPSC<int, 1024> queue;  // capacity rounded up to 1024 (already pow2)

// Producer thread
queue.push(42);                     // spin-wait if full
if (queue.try_push(99)) { ... }     // non-blocking

// Consumer thread
int val = queue.pop();              // spin-wait if empty
auto maybe = queue.try_pop();       // std::optional<int>
```

### Cache-Line Layout

```
Cache line 1 (64 bytes):  [ head_ (std::atomic<size_t>) ] [ padding ... ]
                            ↑ written by producer, read by consumer

Cache line 2 (64 bytes):  [ tail_ (std::atomic<size_t>) ] [ padding ... ]
                            ↑ written by consumer, read by producer
```

### Memory Ordering on ARM

| Operation | Ordering | ARM Instruction | Purpose |
|---|---|---|---|
| Producer reads `head_` | `relaxed` | plain LDR | Producer owns it, no other writer |
| Producer reads `tail_` | `acquire` | LDAR | See consumer's prior slot destruction |
| Producer writes `head_` | `release` | STLR | Publish element construction to consumer |
| Consumer reads `tail_` | `relaxed` | plain LDR | Consumer owns it, no other writer |
| Consumer reads `head_` | `acquire` | LDAR | See producer's prior element construction |
| Consumer writes `tail_` | `release` | STLR | Publish slot vacation to producer |

## ring_buffer_lockfree_CAS_MPMC.hpp

Lock-free bounded ring buffer supporting **multiple producers and multiple consumers** via CAS (compare-and-swap) loops. No mutexes, no condition variables.

- Producers race for `head_` via `compare_exchange_weak` — CAS winner owns the slot
- Consumers race for `tail_` via `compare_exchange_weak` — same pattern
- **Per-slot sequence number** prevents ABA and acts as a slot state machine
- `head_` and `tail_` on separate cache lines
- Monotonically increasing indices (never wrap to 0) — eliminates ABA on the counters
- Capacity rounded to power of 2, index masked only at slot access

### How the CAS Loop Works

```
Producer try_push():
  1. pos = head_.load(relaxed)
  2. slot = slots_[pos & Mask]
  3. if slot.seq == pos:                    // slot is FREE at this turn
       CAS head_ from pos to pos+1
         success → construct element, set slot.seq = pos+1, return true
         fail    → pos reloaded by CAS, goto 2
     if slot.seq < pos:                     // consumer hasn't freed slot → FULL
       return false
     if slot.seq > pos:                     // stale head_, reload
       goto 1

Consumer try_pop():  (mirror image)
  1. pos = tail_.load(relaxed)
  2. slot = slots_[pos & Mask]
  3. if slot.seq == pos+1:                  // slot is FULL at this turn
       CAS tail_ from pos to pos+1
         success → move element out, set slot.seq = pos+Capacity, return value
         fail    → pos reloaded, goto 2
     if slot.seq < pos+1:                   // producer hasn't filled slot → EMPTY
       return nullopt
     if slot.seq > pos+1:                   // stale tail_, reload
       goto 1
```

### Per-Slot Sequence Lifecycle

```
Initial:          seq = i              (slot i is empty, ready for turn i)
After produce:    seq = pos + 1        (slot is full, consumer can read)
After consume:    seq = pos + Capacity (slot is empty, ready for next lap)

The sequence monotonically increases → no ABA.
```

### Usage

```cpp
#include "ring_buffer_lockfree_CAS_MPMC.hpp"

low_latency::LockfreeCAS_MPMC<int, 1024> queue;

// Any producer thread
queue.push(42);                     // spin-wait if full
if (queue.try_push(99)) { ... }     // non-blocking

// Any consumer thread
int val = queue.pop();              // spin-wait if empty
auto maybe = queue.try_pop();       // std::optional<int>
```

## seqslot_MPMC.hpp

Lock-free bounded MPMC ring buffer — **Dmitry Vyukov style**. Slot claim uses `fetch_add` (single atomic RMW, always succeeds, no CAS retry loop). The thread then spins only on the per-slot sequence number reaching the expected value.

- **No CAS retry loop** — `fetch_add` on `head_`/`tail_` always succeeds in one shot
- Spin is purely on per-slot `seq` reaching expected value (slot readiness)
- Per-slot sequence number as state machine (same lifecycle as CAS variant)
- `head_` and `tail_` on separate cache lines
- Under moderate contention, faster than CAS-retry (no CAS storm)

### How It Works (fetch_add + Slot Spin)

```
Producer push():                              Consumer pop():
  pos = head_.fetch_add(1)  ← always succeeds   pos = tail_.fetch_add(1)  ← always succeeds
  spin until slot.seq == pos  ← wait FREE        spin until slot.seq == pos+1  ← wait FULL
  construct element                               move element out, destroy
  slot.seq = pos + 1          ← mark FULL         slot.seq = pos + Cap      ← mark FREE
```

The critical insight: **`fetch_add` never fails** — it returns the old value and atomically increments in one instruction (`LDADDAL` on ARM, `LOCK XADD` on x86). No compare-exchange loop means no wasted iterations on contention. The only spin is waiting for the slot itself to be ready.

### CAS-retry vs fetch_add+spin

```
CAS-retry (ring_buffer_lockfree_CAS_MPMC):
  Thread A: load head=5, CAS(5→6) SUCCESS → owns slot 5
  Thread B: load head=5, CAS(5→6) FAIL    → reload head=6, CAS(6→7) SUCCESS → owns slot 6
  Thread C: load head=5, CAS(5→6) FAIL    → reload head=6, CAS(6→7) FAIL → reload...
  Under contention: multiple retries before success. CAS storm.

fetch_add+spin (seqslot_MPMC):
  Thread A: fetch_add → gets 5, owns slot 5 unconditionally
  Thread B: fetch_add → gets 6, owns slot 6 unconditionally
  Thread C: fetch_add → gets 7, owns slot 7 unconditionally
  Under contention: ZERO retries. Each thread spins only on its own slot's seq.
```

### Trade-off: push Cannot Back Out

Once `fetch_add` increments `head_`, the producer **must** eventually write to that slot. It cannot return false and "undo" the increment. This means:

- `push()` always succeeds but may spin (if buffer is full, waits for consumer)
- `try_push()` peeks at the slot seq before committing — returns false if the slot doesn't look ready, but under contention may still briefly spin after fetch_add

### Usage

```cpp
#include "seqslot_MPMC.hpp"

low_latency::SeqSlotMPMC<int, 1024> queue;

// Any producer thread
queue.push(42);                     // fetch_add + spin on slot

// Any consumer thread
int val = queue.pop();              // fetch_add + spin on slot

// Non-blocking (advisory peek before commit)
if (queue.try_push(99)) { ... }
auto maybe = queue.try_pop();       // std::optional<int>
```

## Comparison

| | `mutex_ring_buffer` | `lockfree_spsc` | `lockfree_CAS_MPMC` | `seqslot_MPMC` |
|---|---|---|---|---|
| Producers | Many | **1 only** | Many | Many |
| Consumers | Many | **1 only** | Many | Many |
| Synchronization | mutex + condvars | atomics (acq/rel) | atomics (CAS loop) | atomics (**fetch_add** + slot spin) |
| Slot claim | lock acquire | relaxed load (owner) | `compare_exchange_weak` | **`fetch_add`** (always succeeds) |
| Blocking push/pop | sleep-wait | spin-wait | spin-wait | spin-wait (on slot seq) |
| try_push can back out | yes | yes | **yes** (CAS fails = no commit) | **no** (fetch_add is irreversible) |
| Timed wait | yes | no | no | no |
| Contention behavior | kernel futex | **none** (1:1) | CAS retry storm | **no retries**, each thread spins on own slot |
| Best for | low-CPU blocking | **ultra-low latency** 1:1 | non-blocking try semantics N:M | **lowest contention** N:M |

## Build

Header-only — just add the project root to your include path:

```
g++ -std=c++17 -I/path/to/cpu_low_latency_runtime -pthread your_app.cpp
```

## License

TBD
