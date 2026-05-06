// xoshiro256** — period 2^256 - 1, passes BigCrush.
// Only headers used: <cstdint> (fixed-width types), <cstdio> (seed from /dev/urandom).
// No <random>, no rand(), no external PRNG library.

#include <cstdint>
#include <cstdio>

namespace prng {

// 64-bit rotate-left. Compilers fold this to a single rol instruction.
static inline uint64_t rotl(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

// ---------------------------------------------------------------------------
// SplitMix64 — bit-mixer used ONLY to expand one seed into 4 state words.
// Why: xoshiro cannot start at all-zero. SplitMix64 has full period 2^64 and
// avalanche quality strong enough that any input (including 0) produces
// well-scattered output.
// ---------------------------------------------------------------------------
struct SplitMix64 {
    uint64_t s;
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ULL);   // odd multiple of 2^64/phi
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
};

// ---------------------------------------------------------------------------
// xoshiro256** — Blackman & Vigna, 2018.
// Output is s[1] * 5 rotated and * 9 — the "**" scrambler hides linear
// structure of the underlying xor-shift-rotate state transition.
// ---------------------------------------------------------------------------
struct Xoshiro256ss {
    uint64_t s[4];

    void seed(uint64_t seed64) {
        SplitMix64 sm{seed64};
        s[0] = sm.next();
        s[1] = sm.next();
        s[2] = sm.next();
        s[3] = sm.next();
        if ((s[0] | s[1] | s[2] | s[3]) == 0) s[0] = 1;  // defensive
    }

    uint64_t next() {
        const uint64_t result = rotl(s[1] * 5, 7) * 9;
        const uint64_t t      = s[1] << 17;

        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3]  = rotl(s[3], 45);

        return result;
    }

    // Uniform double in [0, 1): take top 53 bits of a uint64, scale by 2^-53.
    // Top bits are used because xoshiro's low bits are linearly weaker.
    double next_double() {
        return (next() >> 11) * (1.0 / 9007199254740992.0);   // 9007... = 2^53
    }

    // Uniform uint64 in [0, bound) — Lemire's nearly-divisionless method.
    // Unbiased: rejects samples in the short incomplete interval.
    uint64_t next_bounded(uint64_t bound) {
        __uint128_t m = (__uint128_t)next() * (__uint128_t)bound;
        uint64_t lo = (uint64_t)m;
        if (lo < bound) {
            uint64_t threshold = (uint64_t)(-bound) % bound;  // = (2^64 - bound) mod bound
            while (lo < threshold) {
                m  = (__uint128_t)next() * (__uint128_t)bound;
                lo = (uint64_t)m;
            }
        }
        return (uint64_t)(m >> 64);
    }
};

// ---------------------------------------------------------------------------
// OS entropy seed. /dev/urandom is nonblocking kernel CSPRNG (ChaCha20 on
// Linux >=4.8). We use it only to seed a fast non-crypto PRNG.
// ---------------------------------------------------------------------------
static inline uint64_t os_seed() {
    FILE* f = std::fopen("/dev/urandom", "rb");
    if (!f) return 0xCAFEBABEDEADBEEFULL;        // fallback for embedded
    uint64_t s = 0;
    size_t n = std::fread(&s, sizeof(s), 1, f);
    std::fclose(f);
    return n == 1 ? s : 0xCAFEBABEDEADBEEFULL;
}

} // namespace prng

// ---------------------------------------------------------------------------
// Demo
// ---------------------------------------------------------------------------
int main() {
    prng::Xoshiro256ss rng;
    rng.seed(prng::os_seed());

    std::printf("raw u64:\n");
    for (int i = 0; i < 5; ++i)
        std::printf("  %016llx\n", (unsigned long long)rng.next());

    std::printf("double in [0,1):\n");
    for (int i = 0; i < 5; ++i)
        std::printf("  %.17f\n", rng.next_double());

    std::printf("uniform in [0,100):\n");
    for (int i = 0; i < 5; ++i)
        std::printf("  %llu\n", (unsigned long long)rng.next_bounded(100));
}
