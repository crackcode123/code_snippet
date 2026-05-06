// base64 encoder — per-case (1, 2, 3 byte) handling made explicit.
//
// Three operations for every case:
//   PACK  : concatenate input bytes, MSB-first, into the low bits of a word.
//   SLICE : extract consecutive 6-bit indices by (shift right, mask 0x3F).
//   MAP   : look up each 6-bit index in the 64-char alphabet.
//
// Output padding ('=') serves the decoder, not the encoder: it carries the
// original byte count modulo 3 when the output length alone is insufficient.
//
// Reference: RFC 4648 §4 (standard base64).

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// RFC 4648 standard alphabet.
//   indices   0..25  -> 'A'..'Z'
//   indices  26..51  -> 'a'..'z'
//   indices  52..61  -> '0'..'9'
//   index        62  -> '+'
//   index        63  -> '/'
static const char A64[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// ---------------------------------------------------------------------------
// Case: 3 input bytes (24 data bits, no zero-pad needed).
//       Output: 4 alphabet chars, no '=' tail.
// ---------------------------------------------------------------------------
static void enc_3(const uint8_t in[3], char out[4]) {
    // PACK — 3 bytes into the low 24 bits of v, MSB-first:
    //   bit layout of v (bit 31 -> bit 0):
    //     [ 0 0 0 0 0 0 0 0 | in[0] | in[1] | in[2] ]
    //                         ↑ [23..16]  ↑ [15..8]  ↑ [7..0]
    uint32_t v = (uint32_t(in[0]) << 16)
               | (uint32_t(in[1]) <<  8)
               |  uint32_t(in[2]);

    // SLICE — cut bits [23..0] into four 6-bit slices, MSB-first.
    //   s0 = v[23..18]   (top 6 bits of in[0])
    //   s1 = v[17..12]   (in[0] low 2 bits + in[1] high 4 bits)
    //   s2 = v[11..6]    (in[1] low 4 bits + in[2] high 2 bits)
    //   s3 = v[5..0]     (low 6 bits of in[2])
    uint32_t s0 = (v >> 18) & 0x3F;
    uint32_t s1 = (v >> 12) & 0x3F;
    uint32_t s2 = (v >>  6) & 0x3F;
    uint32_t s3 =  v        & 0x3F;

    // MAP — each 6-bit index -> printable base64 char.
    out[0] = A64[s0];
    out[1] = A64[s1];
    out[2] = A64[s2];
    out[3] = A64[s3];
}

// ---------------------------------------------------------------------------
// Case: 2 input bytes (16 data bits + 2 zero-pad bits = 18, → 3 slices).
//       Output: 3 alphabet chars + 1 '='.
// ---------------------------------------------------------------------------
static void enc_2(const uint8_t in[2], char out[4]) {
    // PACK — 2 bytes into positions [23..16] and [15..8]. Positions [7..0]
    //        remain zero; the 2 zero-pad bits we actually consume live in
    //        v[7..6], the rest (v[5..0]) are silently discarded because we
    //        emit only 3 slices.
    //     [ 0 0 0 0 0 0 0 0 | in[0] | in[1] | 0 0 0 0 0 0 0 0 ]
    uint32_t v = (uint32_t(in[0]) << 16)
               | (uint32_t(in[1]) <<  8);

    // SLICE — three 6-bit slices:
    //   s0 = v[23..18]   (top 6 bits of in[0])                   — all real
    //   s1 = v[17..12]   (in[0] low 2 bits + in[1] high 4 bits)  — all real
    //   s2 = v[11..6]    (in[1] low 4 bits + 2 zero-pad bits)
    uint32_t s0 = (v >> 18) & 0x3F;
    uint32_t s1 = (v >> 12) & 0x3F;
    uint32_t s2 = (v >>  6) & 0x3F;

    // MAP + emit '=' to signal "one tail char was zero-pad-only".
    out[0] = A64[s0];
    out[1] = A64[s1];
    out[2] = A64[s2];
    out[3] = '=';
}

// ---------------------------------------------------------------------------
// Case: 1 input byte (8 data bits + 4 zero-pad bits = 12, → 2 slices).
//       Output: 2 alphabet chars + 2 '='.
// ---------------------------------------------------------------------------
static void enc_1(const uint8_t in[1], char out[4]) {
    // PACK — single byte at positions [23..16]. All lower bits are zero.
    //     [ 0 0 0 0 0 0 0 0 | in[0] | 0 0 0 0 0 0 0 0 | 0 0 0 0 0 0 0 0 ]
    uint32_t v = uint32_t(in[0]) << 16;

    // SLICE — two 6-bit slices:
    //   s0 = v[23..18]   (top 6 bits of in[0])                   — all real
    //   s1 = v[17..12]   (in[0] low 2 bits + 4 zero-pad bits)
    uint32_t s0 = (v >> 18) & 0x3F;
    uint32_t s1 = (v >> 12) & 0x3F;

    // MAP + emit "==" to signal "two tail chars worth of zero-pad".
    out[0] = A64[s0];
    out[1] = A64[s1];
    out[2] = '=';
    out[3] = '=';
}

// ---------------------------------------------------------------------------
// Public encoder: dispatches to enc_3 for full groups, then to enc_1 / enc_2
// for the tail (0, 1, or 2 leftover bytes).
// ---------------------------------------------------------------------------
std::string base64_encode(const uint8_t* src, size_t n) {
    std::string out;
    out.reserve(((n + 2) / 3) * 4);     // ceil(n/3) * 4 chars

    char chunk[4];
    size_t i = 0;

    // Full 3-byte groups.
    for (; i + 3 <= n; i += 3) {
        enc_3(&src[i], chunk);
        out.append(chunk, 4);
    }

    // Tail: 0, 1, or 2 leftover bytes.
    size_t rem = n - i;
    if (rem == 1) {
        enc_1(&src[i], chunk);
        out.append(chunk, 4);
    } else if (rem == 2) {
        enc_2(&src[i], chunk);
        out.append(chunk, 4);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Self-test — exercises all three tail cases plus an empty input.
// ---------------------------------------------------------------------------
int main() {
    struct { const char* in; const char* expect; } cases[] = {
        { "",      ""         },     // 0 bytes
        { "M",     "TQ=="     },     // 1-byte tail
        { "Ma",    "TWE="     },     // 2-byte tail
        { "Man",   "TWFu"     },     // exact 3-byte group
        { "Many",  "TWFueQ==" },     // 3 + 1
        { "Manny", "TWFubnk=" },     // 3 + 2
    };

    int pass = 0, total = 0;
    for (const auto& c : cases) {
        total++;
        std::string got = base64_encode(
            reinterpret_cast<const uint8_t*>(c.in),
            std::strlen(c.in));
        bool ok = (got == c.expect);
        if (ok) pass++;
        std::printf("%-8s -> %-10s  expect %-10s  %s\n",
                    c.in, got.c_str(), c.expect, ok ? "OK" : "FAIL");
    }
    std::printf("%d/%d passed\n", pass, total);
    return (pass == total) ? 0 : 1;
}
