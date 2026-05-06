#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>

// Flat hash would be better (Swiss table / Robin Hood) but stdlib for clarity.
// Real impl: replace with absl::flat_hash_map for ~3ns/probe vs ~15ns here.

struct PairHash {
    size_t operator()(const std::pair<uint32_t,uint32_t>& p) const {
        return std::hash<uint64_t>{}((uint64_t)p.first << 32 | p.second);
    }
};

struct BPETokenizer {
    // merge table: (token_a, token_b) -> priority rank (lower = merge first)
    std::unordered_map<std::pair<uint32_t,uint32_t>, uint32_t, PairHash> merges;

    // token_id -> byte string
    std::vector<std::string> id_to_bytes;

    // byte string -> token_id
    std::unordered_map<std::string, uint32_t> bytes_to_id;

    void add_byte_tokens() {
        // base vocab: 256 single-byte tokens
        for (int b = 0; b < 256; b++) {
            std::string s(1, (char)b);
            uint32_t id = (uint32_t)id_to_bytes.size();
            id_to_bytes.push_back(s);
            bytes_to_id[s] = id;
        }
    }

    // add a merge rule: merging token_a + token_b produces a new token
    // priority = order added (lower = higher priority)
    uint32_t add_merge(const std::string& a, const std::string& b) {
        uint32_t id_a = bytes_to_id[a];
        uint32_t id_b = bytes_to_id[b];
        std::string merged = a + b;
        uint32_t new_id = (uint32_t)id_to_bytes.size();
        uint32_t rank = (uint32_t)merges.size();

        id_to_bytes.push_back(merged);
        bytes_to_id[merged] = new_id;
        merges[{id_a, id_b}] = rank;
        return new_id;
    }

    // BPE encode: bytes -> token ids
    std::vector<uint32_t> encode(const std::string& text) {
        // step 0: split into byte-level tokens
        std::vector<uint32_t> tokens;
        for (unsigned char c : text) {
            tokens.push_back((uint32_t)c);
        }

        // step 1: repeatedly merge the highest-priority pair
        while (tokens.size() > 1) {
            // find the pair with lowest rank (highest priority)
            uint32_t best_rank = UINT32_MAX;
            size_t best_pos = 0;
            bool found = false;

            for (size_t i = 0; i + 1 < tokens.size(); i++) {
                auto it = merges.find({tokens[i], tokens[i+1]});
                if (it != merges.end() && it->second < best_rank) {
                    best_rank = it->second;
                    best_pos = i;
                    found = true;
                }
            }

            if (!found) break;

            // merge: replace pair at best_pos with the merged token
            uint32_t merged_id = bytes_to_id[
                id_to_bytes[tokens[best_pos]] + id_to_bytes[tokens[best_pos+1]]
            ];
            tokens[best_pos] = merged_id;
            tokens.erase(tokens.begin() + best_pos + 1);
        }

        return tokens;
    }

    // decode: token ids -> string
    std::string decode(const std::vector<uint32_t>& ids) {
        std::string out;
        for (uint32_t id : ids) {
            out += id_to_bytes[id];
        }
        return out;
    }
};

int main() {
    BPETokenizer tok;
    tok.add_byte_tokens(); // 256 base tokens

    // build a small merge table (order = priority)
    tok.add_merge("h", "o");     // rank 0: "ho"
    tok.add_merge("ho", "w");    // rank 1: "how"
    tok.add_merge("a", "r");     // rank 2: "ar"
    tok.add_merge("ar", "e");    // rank 3: "are"
    tok.add_merge("y", "o");     // rank 4: "yo"
    tok.add_merge("yo", "u");    // rank 5: "you"
    tok.add_merge(" ", "are");   // rank 6: " are"
    tok.add_merge(" ", "you");   // rank 7: " you"

    std::string input = "how are you";
    auto ids = tok.encode(input);

    printf("input: \"%s\"\ntokens: [", input.c_str());
    for (size_t i = 0; i < ids.size(); i++) {
        printf("%u(\"%s\")%s", ids[i], tok.id_to_bytes[ids[i]].c_str(),
               i+1 < ids.size() ? ", " : "");
    }
    printf("]\n");

    printf("decoded: \"%s\"\n", tok.decode(ids).c_str());
    return 0;
}
