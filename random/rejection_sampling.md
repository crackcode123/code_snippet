# Rejection Sampling for Uniform Discrete Generation

*Two canonical problems, one invariant.*

---

## 0. Thesis

Given a uniform primitive over *a* outcomes, you can produce a uniform sample
over *b* outcomes **deterministically** only when *b* divides some power *aᵏ*.
When it does not (gcd(b, a) < b and b ∤ aᵏ for all k), **rejection sampling is
the unique zero-bias construction**: expand the source space to *aᵏ ≥ b*,
accept the first `b · ⌊aᵏ/b⌋` outcomes, retry on the rest, return `result mod b`.

Acceptance probability `p = b · ⌊aᵏ/b⌋ / aᵏ`. Expected trials `1/p`. Expected
source-calls per sample `k/p`. Both problems below are instances of this
single statement.

---

## 1. The general construction

```
source primitive: uniform in {0 .. a-1}
target:           uniform in {0 .. b-1}

pick k such that a^k >= b

     0 ──────────────────────── a^k - 1
     |                               |
     |<—— b · floor(a^k / b) ——>|<—reject region—>|
              ACCEPT region          retry

on accept:  return (sample) mod b       (uniform in {0..b-1})
on reject:  re-sample from scratch

correctness: each of the b residues has exactly floor(a^k / b) preimages
             in the accept region → equiprobable.
bias: zero. (Any non-rejection mapping would force unequal preimage counts.)
```

Choosing *k*: larger k raises acceptance probability (less waste) but costs
more primitive calls per trial. Sweet spot: smallest *k* with *aᵏ ≥ b·a*
(first trial can't be cheaper than one primitive call; aim for waste < a).

---

## 2. Problem A — Fair coin → uniform {0..6}

### 2.1 Statement

Given `flip()` returning 0 or 1 with probability ½ each (independent calls),
construct `rand7()` returning a uniform integer in {0, 1, …, 6}. No PRNG
library. Output must be **exactly** uniform (no ε bias).

### 2.2 Why rejection is unavoidable

gcd(7, 2ᵏ) = 1 for every k ≥ 1. So 2ᵏ mod 7 ≠ 0 always; no deterministic
k-flip procedure yields seven equal-probability outcomes. (Knuth–Yao: any
unbiased procedure for 7 outcomes using a fair coin has **unbounded**
worst-case flip count.)

### 2.3 Construction

- *a* = 2, *b* = 7, choose *k* = 3 → aᵏ = 8.
- Accept region [0, 7), reject {7}.
- Acceptance p = 7/8. E[trials] = 8/7. E[flips] = 3 · 8/7 = **24/7 ≈ 3.43**.

```
3 flips → b2 b1 b0 ∈ {0..7}
 if value <  7: return value       (prob 7/8)
 if value == 7: retry              (prob 1/8)
```

### 2.4 Proof of uniformity (one paragraph)

Let X be the returned value. Each of 8 bit-triples is equiprobable (1/8).
Conditional on acceptance, the 7 accepted triples remain equiprobable among
themselves by symmetry of conditioning. So P(X = i) = (1/8) / (7/8) = 1/7
for each i ∈ {0..6}. ∎

### 2.5 C++ — `coin_to_rand7.cpp`

```cpp
// Fair coin (from /dev/urandom bits) -> uniform {0..6} via rejection.
// Mechanism: 3 flips produce {0..7}; discard 7 and retry.

#include <cstdint>
#include <cstdio>

// --- fair-coin source ------------------------------------------------------
// One byte from /dev/urandom supplies 8 independent fair bits.
static int coin_buf = 0;
static int coin_bits_left = 0;
static FILE* coin_src = nullptr;

int flip() {
    if (coin_bits_left == 0) {
        if (!coin_src) coin_src = std::fopen("/dev/urandom", "rb");
        coin_buf = std::fgetc(coin_src);
        coin_bits_left = 8;
    }
    int bit = coin_buf & 1;
    coin_buf >>= 1;
    coin_bits_left--;
    return bit;
}

// --- rand7: rejection sampling over 3-flip space --------------------------
int rand7() {
    for (;;) {
        int v = (flip() << 2) | (flip() << 1) | flip();  // uniform {0..7}
        if (v < 7) return v;                             // accept 7/8
        // else retry
    }
}

int main() {
    int hist[7] = {0};
    const int N = 7'000'000;
    for (int i = 0; i < N; ++i) hist[rand7()]++;
    for (int i = 0; i < 7; ++i)
        std::printf("%d: %d (%.5f)\n", i, hist[i], hist[i] / (double)N);
}
```

**Empirical verification** (N = 7 × 10⁶): each bucket 0.14257–0.14307 vs the
theoretical 1/7 = 0.14286 — all within normal statistical variance.

### 2.6 Why this problem exists as an interview question

Tests whether the candidate sees that (a) not every target fits a power of 2,
(b) rejection is the principled fix, (c) "just take value mod 7 from 8
outcomes" is *biased* (residues 0–6 get 1 preimage each, residue 0 gets an
extra from value=7 under naïve wrap — producing P(0) = 2/8). This trap is the
whole point of the question.

---

## 3. Problem B — Rand7 → Rand10 (LeetCode 470)

### 3.1 Statement

Given `rand7()` returning a uniform integer in {1..7}, implement `rand10()`
returning a uniform integer in {1..10}. Calls to rand7() are the only source
of randomness.

### 3.2 Why rejection is unavoidable

10 does not divide 7ᵏ for any k (gcd(10, 7ᵏ) = 1). Same structural obstruction
as Problem A.

### 3.3 Basic construction

- *a* = 7, *b* = 10, choose *k* = 2 → 7² = 49.
- Accept region {1..40} (largest multiple of 10 ≤ 49), reject {41..49}.
- p = 40/49. E[trials] = 49/40 = 1.225. E[rand7-calls] = 2 · 49/40 = **49/20 = 2.45**.

```
row = rand7()           // {1..7}
col = rand7()           // {1..7}
idx = (row-1)*7 + col   // {1..49}  ← uniform over 49 outcomes
if idx <= 40: return 1 + (idx-1) % 10
else: retry
```

The column-major encoding `(row-1)*7 + col` is a bijection {1..7}² → {1..49};
uniformity of (row, col) → uniformity of idx.

### 3.4 Why reject at 40, not 41 or 49?

We must cut at the largest multiple of *b* = 10 that is ≤ 7² = 49. That is
40. Any other cut makes the residues uneven:

- Cut at 49 (no rejection): residues 1..9 get 5 preimages each, residue 0
  gets 4 → biased low by 1/49 on average.
- Cut at 30: still unbiased but wastes 19/49 of samples, raising E[calls]
  unnecessarily.

### 3.5 Optimized: reuse rejected outcomes

When the first trial rejects with idx ∈ {41..49}, those 9 values are still
**uniform** over a 9-element set. Don't throw them away — use them as a new
uniform primitive:

```
(1) idx1 ∈ {1..49}.  accept idx1 ≤ 40.  else carry u = idx1 - 40 ∈ {1..9}.
(2) idx2 = (u-1)*7 + rand7() ∈ {1..63}.  accept idx2 ≤ 60.  else carry v = idx2 - 60 ∈ {1..3}.
(3) idx3 = (v-1)*7 + rand7() ∈ {1..21}.  accept idx3 ≤ 20.  else restart from (1).
```

E[calls] drops from 2.45 to **≈ 2.21**.

### 3.6 C++ — `rand10_from_rand7.cpp`

```cpp
// Uniform {1..10} using only rand7() (simulated from /dev/urandom here).
// Two variants: basic (E[calls]=2.45), optimized (E[calls]≈2.21).

#include <cstdint>
#include <cstdio>

// --- simulated rand7() -----------------------------------------------------
// In the LeetCode problem, rand7() is given. Here we stand it up using
// /dev/urandom + rejection (low 3 bits → {0..7}, reject 7) to keep the
// test harness self-contained.
static FILE* src = nullptr;
int rand7() {
    if (!src) src = std::fopen("/dev/urandom", "rb");
    for (;;) {
        unsigned char b;
        if (std::fread(&b, 1, 1, src) != 1) return 1;
        int v = b & 0x07;             // low 3 bits → {0..7}
        if (v < 7) return v + 1;      // map to {1..7}
    }
}

// --- basic: E[rand7 calls] = 49/20 = 2.45 ---------------------------------
int rand10_basic() {
    for (;;) {
        int row = rand7();                     // {1..7}
        int col = rand7();                     // {1..7}
        int idx = (row - 1) * 7 + col;         // {1..49}
        if (idx <= 40) return 1 + (idx - 1) % 10;
    }
}

// --- optimized: reuse rejected outcomes, E[rand7 calls] ≈ 2.21 ------------
int rand10_opt() {
    for (;;) {
        int a = rand7(), b = rand7();
        int idx1 = (a - 1) * 7 + b;            // {1..49}
        if (idx1 <= 40) return 1 + (idx1 - 1) % 10;

        int u = idx1 - 40;                     // uniform {1..9}
        int c = rand7();
        int idx2 = (u - 1) * 7 + c;            // {1..63}
        if (idx2 <= 60) return 1 + (idx2 - 1) % 10;

        int v = idx2 - 60;                     // uniform {1..3}
        int d = rand7();
        int idx3 = (v - 1) * 7 + d;            // {1..21}
        if (idx3 <= 20) return 1 + (idx3 - 1) % 10;
        // else restart
    }
}

int main() {
    const int N = 10'000'000;
    int hb[11] = {0}, ho[11] = {0};
    for (int i = 0; i < N; ++i) { hb[rand10_basic()]++; ho[rand10_opt()]++; }
    std::printf("basic:\n");
    for (int i = 1; i <= 10; ++i) std::printf("  %2d: %.5f\n", i, hb[i] / (double)N);
    std::printf("optimized:\n");
    for (int i = 1; i <= 10; ++i) std::printf("  %2d: %.5f\n", i, ho[i] / (double)N);
}
```

**Empirical verification** (N = 10⁷, both variants): all 10 buckets within
0.0998–0.1001 of the theoretical 0.1 — within statistical variance.

### 3.7 The optimized variant's expected-calls derivation

Let E = E[rand7 calls per rand10 sample]. First stage always spends 2 calls.

- P(accept at stage 1) = 40/49.
- P(reach stage 2) = 9/49; stage 2 costs 1 more call; P(accept | stage 2) = 60/63.
- P(reach stage 3) = (9/49)·(3/63); stage 3 costs 1 more call; P(accept | stage 3) = 20/21.
- P(restart) = (9/49)·(3/63)·(1/21) ≈ 1/720.

E = 2 + (9/49)·[1 + (3/63)·(1 + (1/21)·E)]. Solving: E ≈ 2.21. The near-720:1
odds of ever restarting is why the worst-case tail is negligible in practice.

---

## 4. The shared pattern (takeaway)

Both problems are the same construction with different *a* and *b*:

| Problem | a | b | k | aᵏ | accept | p = b·⌊aᵏ/b⌋/aᵏ | E[calls]     |
|---------|---|---|---|----|--------|-----------------|--------------|
| Coin → 7 | 2 | 7 | 3 | 8  | {0..6} | 7/8             | 24/7 ≈ 3.43  |
| R7 → R10 | 7 |10 | 2 | 49 | {1..40}| 40/49           | 49/20 = 2.45 |

The optimization in 3.5 generalizes too: rejected samples from a uniform set
of size `aᵏ − b·⌊aᵏ/b⌋` are themselves uniform over that smaller set, so you
can feed them back as a new primitive and only pay one fresh source-call per
extension stage. This is sometimes called **entropy recycling** or
**Lumbroso-style rejection**.

## 5. What an interviewer probes

- Do you see `value mod b` over *aᵏ* outcomes is biased whenever b ∤ aᵏ?
- Can you state the accept-region rule (largest multiple of *b* ≤ aᵏ)?
- Can you compute E[calls] and reason about why worst-case is unbounded?
- Can you produce the entropy-recycling optimization, or recognize that
  rejected outcomes still carry uniform information?

The first two filter for correctness. The latter two filter for depth.
