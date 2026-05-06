#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

static constexpr uint32_t PAD_ID = 0;

struct StaticBatcher {
    // Pad all sequences to the same length, form a rectangular batch.
    // Returns: [batch_size x max_len] token matrix + attention mask
    struct Batch {
        size_t batch_size;
        size_t max_len;
        std::vector<uint32_t> token_ids;   // [batch_size * max_len], row-major
        std::vector<uint8_t>  attn_mask;   // 1 = real token, 0 = pad
    };

    static Batch make_batch(const std::vector<std::vector<uint32_t>>& sequences) {
        Batch b;
        b.batch_size = sequences.size();
        b.max_len = 0;
        for (auto& seq : sequences) {
            b.max_len = std::max(b.max_len, seq.size());
        }

        b.token_ids.resize(b.batch_size * b.max_len, PAD_ID);
        b.attn_mask.resize(b.batch_size * b.max_len, 0);

        for (size_t i = 0; i < b.batch_size; i++) {
            for (size_t j = 0; j < sequences[i].size(); j++) {
                b.token_ids[i * b.max_len + j] = sequences[i][j];
                b.attn_mask[i * b.max_len + j] = 1;
            }
            // remaining positions already PAD_ID and mask=0
        }
        return b;
    }

    static void print_batch(const Batch& b) {
        printf("static batch: %zu seqs x %zu max_len\n", b.batch_size, b.max_len);
        for (size_t i = 0; i < b.batch_size; i++) {
            printf("  seq %zu: [", i);
            for (size_t j = 0; j < b.max_len; j++) {
                uint32_t tok = b.token_ids[i * b.max_len + j];
                uint8_t  msk = b.attn_mask[i * b.max_len + j];
                if (msk) printf(" %u", tok);
                else     printf(" _");  // pad
            }
            printf(" ]\n");
        }

        // compute waste
        size_t total = b.batch_size * b.max_len;
        size_t pad_count = 0;
        for (auto m : b.attn_mask) pad_count += (m == 0);
        printf("  waste: %zu/%zu = %.1f%% of compute is padding\n",
               pad_count, total, 100.0 * pad_count / total);
    }
};

int main() {
    // 4 sequences of very different lengths — worst case for static batching
    std::vector<std::vector<uint32_t>> sequences = {
        {10, 20, 30},                                         // 3 tokens
        {40, 50, 60, 70, 80, 90, 100, 110},                 // 8 tokens
        {1, 2},                                               // 2 tokens
        {200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
         210, 211, 212, 213, 214, 215},                      // 16 tokens
    };

    auto batch = StaticBatcher::make_batch(sequences);
    StaticBatcher::print_batch(batch);
    // GPU gets a [4 x 16] matrix. Rows with 2-3 real tokens pay for 16.
    // attention mask prevents pad tokens from affecting output,
    // but the matmul FLOPs are wasted.
    return 0;
}
