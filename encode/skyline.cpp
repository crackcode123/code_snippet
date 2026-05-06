// Skyline problem — LeetCode 218.
//
// Input : buildings as triples [L, R, H] (left x, right x, height).
// Output: skyline as a sequence of (x, h) key points at which the outline
//         changes height. Final point always ends at h=0 (ground).
//
// Algorithm — plane sweep + max-multiset:
//   1. Turn each building into two events:
//        start: (L, -H)    // negative height marks a START
//        end  : (R, +H)    // positive height marks an END
//      Using signed heights makes the x-tie ordering correct under a
//      plain lexicographic sort:
//        - At same x: starts (negative) precede ends (positive).
//          Rationale: inserting before removing avoids spurious drops when
//          one building starts exactly where another ends.
//        - Among simultaneous starts at same x: taller first (-H1 < -H2
//          iff H1 > H2), so the skyline jumps straight to the tallest.
//        - Among simultaneous ends at same x: shorter first, so the taller
//          active height carries the max until its own end event.
//   2. Maintain a multiset of currently-active heights, seeded with 0 so
//      the max is always defined.
//   3. For each event in sorted order:
//        START: insert H
//        END  : erase one occurrence of H
//      After update, read cur_max = *active.rbegin(). If it differs from
//      the previously-emitted height, emit (x, cur_max).
//
// Complexity: O(n log n) time, O(n) space.

#include <algorithm>
#include <cstdio>
#include <set>
#include <utility>
#include <vector>

using Building = std::vector<int>;   // [L, R, H]
using Point    = std::vector<int>;   // [x, h]

std::vector<Point> getSkyline(const std::vector<Building>& buildings) {
    // (x, signed_height): -H for start, +H for end.
    std::vector<std::pair<int,int>> events;
    events.reserve(buildings.size() * 2);
    for (const auto& b : buildings) {
        int L = b[0], R = b[1], H = b[2];
        events.emplace_back(L, -H);
        events.emplace_back(R,  H);
    }
    std::sort(events.begin(), events.end());

    std::multiset<int> active{0};    // floor of 0 so rbegin() is always valid
    std::vector<Point> out;
    int prev_max = 0;

    for (auto [x, sh] : events) {
        if (sh < 0) {
            active.insert(-sh);                  // START
        } else {
            active.erase(active.find(sh));       // END (remove one copy only)
        }
        int cur_max = *active.rbegin();
        if (cur_max != prev_max) {
            out.push_back({x, cur_max});
            prev_max = cur_max;
        }
    }
    return out;
}

int main() {
    // LeetCode 218 canonical example.
    std::vector<Building> b = {
        {2, 9, 10}, {3, 7, 15}, {5, 12, 12}, {15, 20, 10}, {19, 24, 8}
    };
    auto sk = getSkyline(b);

    // Expected: [[2,10],[3,15],[7,12],[12,0],[15,10],[20,8],[24,0]]
    std::vector<Point> expected = {
        {2,10}, {3,15}, {7,12}, {12,0}, {15,10}, {20,8}, {24,0}
    };

    std::printf("got:     ");
    for (auto& p : sk)       std::printf("[%d,%d] ", p[0], p[1]);
    std::printf("\nexpect:  ");
    for (auto& p : expected) std::printf("[%d,%d] ", p[0], p[1]);
    std::printf("\n%s\n", (sk == expected) ? "OK" : "FAIL");
    return (sk == expected) ? 0 : 1;
}
