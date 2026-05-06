// Minimal geohash encoder.
//
// Algorithm:
//   1. Binary-search the longitude range [-180, 180]. Record bit 1 if lon
//      is in the upper half, 0 if lower. Narrow the range. Repeat.
//   2. Binary-search the latitude  range [ -90,  90] the same way.
//   3. Interleave bits, LONGITUDE first: lon0 lat0 lon1 lat1 lon2 lat2 ...
//   4. Group into 5-bit chunks, map each via Niemeyer's base-32 alphabet:
//        "0123456789bcdefghjkmnpqrstuvwxyz"
//      (digits + lowercase minus a, i, l, o — visually unambiguous).
//   5. Stop when `precision` chars emitted.
//
// Equivalent to: a linearized quadtree with lon-first bit-interleaving,
// then base-32 string encoding. Shared prefix <=> nearby cell.

#include <cstdint>
#include <cstdio>
#include <string>

// Niemeyer's base-32 alphabet. NOT RFC 4648.
static const char A32[] = "0123456789bcdefghjkmnpqrstuvwxyz";

std::string geohash_encode(double lat, double lon, int precision) {
    double lat_lo = -90.0,  lat_hi = 90.0;
    double lon_lo = -180.0, lon_hi = 180.0;

    std::string out;
    out.reserve(precision);

    uint32_t bits = 0;      // 5-bit accumulator for current char
    int      nbit = 0;      // how many bits accumulated
    bool     lon_turn = true;   // geohash emits longitude bit first

    while ((int)out.size() < precision) {
        int bit;
        if (lon_turn) {
            double mid = (lon_lo + lon_hi) * 0.5;
            if (lon >= mid) { bit = 1; lon_lo = mid; }
            else            { bit = 0; lon_hi = mid; }
        } else {
            double mid = (lat_lo + lat_hi) * 0.5;
            if (lat >= mid) { bit = 1; lat_lo = mid; }
            else            { bit = 0; lat_hi = mid; }
        }
        lon_turn = !lon_turn;

        // Shift bit into accumulator; flush to a char once 5 bits are in.
        bits = (bits << 1) | uint32_t(bit);
        nbit++;
        if (nbit == 5) {
            out += A32[bits];
            bits = 0;
            nbit = 0;
        }
    }
    return out;
}

int main() {
    struct { double lat, lon; int prec; const char* expect; } cases[] = {
        // Niemeyer's reference example (Jutland, Denmark).
        { 57.64911,   10.40744, 12, "u4pruydqqvj8" },
        // San Francisco (37.7749 N, 122.4194 W) — widely cited example.
        { 37.7749,  -122.4194,  9, "9q8yyk8yt"    },
        // Null Island.
        {  0.0,      0.0,       8, "s0000000"     },
    };

    int pass = 0, total = 0;
    for (const auto& c : cases) {
        total++;
        std::string got = geohash_encode(c.lat, c.lon, c.prec);
        bool ok = (got == c.expect);
        if (ok) pass++;
        std::printf("(%+9.4f,%+10.4f) p=%2d -> %-14s expect %-14s %s\n",
                    c.lat, c.lon, c.prec, got.c_str(), c.expect,
                    ok ? "OK" : "FAIL");
    }
    std::printf("%d/%d passed\n", pass, total);
    return (pass == total) ? 0 : 1;
}
