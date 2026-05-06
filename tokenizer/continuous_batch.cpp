#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <queue>

// Continuous batching: no padding. Sequences enter/leave the batch independently.
// Each iteration, the scheduler picks which tokens to run. Prefill and decode
// tokens from different sequences can share the same GPU step.

struct Sequence {
    uint32_t seq_id;
    std::vector<uint32_t> prompt_ids;  // input tokens (need prefill)
    std::vector<uint32_t> output_ids;  // generated so far
    size_t prefill_done;               // how many prompt tokens prefilled
    size_t max_gen;                    // stop after this many generated tokens
    bool finished;

    bool needs_prefill() const { return prefill_done < prompt_ids.size(); }
    size_t total_len() const { return prompt_ids.size() + output_ids.size(); }
};

struct SchedulerStep {
    // tokens to process this GPU iteration — mix of prefill and decode
    struct TokenSlot {
        uint32_t seq_id;
        uint32_t token_id;
        size_t   position;    // position in the sequence (for RoPE / KV write)
        bool     is_prefill;  // prefill token vs decode token
    };
    std::vector<TokenSlot> slots;
};

struct ContinuousBatchScheduler {
    std::queue<Sequence*> waiting;     // haven't started prefill yet
    std::vector<Sequence*> active;     // in the batch (prefilling or decoding)
    size_t max_batch_tokens;           // max tokens per GPU step (memory bound)
    size_t max_active_seqs;            // max concurrent sequences (KV slots)

    ContinuousBatchScheduler(size_t max_tokens, size_t max_seqs)
        : max_batch_tokens(max_tokens), max_active_seqs(max_seqs) {}

    void add_request(Sequence* seq) {
        waiting.push(seq);
    }

    // build one GPU step: decide which tokens to process
    SchedulerStep schedule() {
        SchedulerStep step;
        size_t budget = max_batch_tokens;

        // 1. remove finished sequences — their KV slots free up
        for (auto it = active.begin(); it != active.end(); ) {
            if ((*it)->finished) it = active.erase(it);
            else ++it;
        }

        // 2. admit new sequences from waiting queue if we have KV slots
        while (!waiting.empty() && active.size() < max_active_seqs) {
            active.push_back(waiting.front());
            waiting.pop();
        }

        // 3. schedule prefill tokens (chunked: don't blow the budget)
        for (auto* seq : active) {
            if (!seq->needs_prefill() || budget == 0) continue;

            size_t remaining = seq->prompt_ids.size() - seq->prefill_done;
            size_t chunk = std::min(remaining, budget);

            for (size_t i = 0; i < chunk; i++) {
                step.slots.push_back({
                    seq->seq_id,
                    seq->prompt_ids[seq->prefill_done + i],
                    seq->prefill_done + i,
                    true
                });
            }
            seq->prefill_done += chunk;
            budget -= chunk;
        }

        // 4. schedule decode tokens (one per active sequence that finished prefill)
        for (auto* seq : active) {
            if (seq->needs_prefill() || seq->finished || budget == 0) continue;

            // simulate: the "generated" token would come from GPU argmax.
            // here we fake it with a counter.
            uint32_t fake_token = 9000 + (uint32_t)seq->output_ids.size();
            seq->output_ids.push_back(fake_token);

            step.slots.push_back({
                seq->seq_id,
                fake_token,
                seq->total_len() - 1,
                false
            });
            budget--;

            if (seq->output_ids.size() >= seq->max_gen) {
                seq->finished = true;
            }
        }

        return step;
    }

    bool all_done() const {
        return waiting.empty() && active.empty();
    }
};

void print_step(int iter, const SchedulerStep& step) {
    size_t n_prefill = 0, n_decode = 0;
    for (auto& s : step.slots) {
        if (s.is_prefill) n_prefill++;
        else n_decode++;
    }
    printf("step %d: %zu tokens (%zu prefill + %zu decode)\n",
           iter, step.slots.size(), n_prefill, n_decode);
    for (auto& s : step.slots) {
        printf("  seq=%u pos=%zu tok=%u %s\n",
               s.seq_id, s.position, s.token_id,
               s.is_prefill ? "[prefill]" : "[decode]");
    }
}

int main() {
    // max 8 tokens per GPU step, max 4 concurrent sequences
    ContinuousBatchScheduler sched(8, 4);

    // 3 requests arrive with different prompt lengths
    Sequence s0 = {0, {10, 20, 30}, {}, 0, 3, false};              // 3-tok prompt, gen 3
    Sequence s1 = {1, {40, 50, 60, 70, 80, 90, 100}, {}, 0, 2, false}; // 7-tok prompt, gen 2
    Sequence s2 = {2, {200, 201}, {}, 0, 2, false};                 // 2-tok prompt, gen 2

    sched.add_request(&s0);
    sched.add_request(&s1);
    sched.add_request(&s2);

    // run the scheduler loop
    printf("=== continuous batching ===\n");
    printf("budget: 8 tokens/step, 4 max seqs\n\n");

    for (int iter = 0; !sched.all_done(); iter++) {
        auto step = sched.schedule();
        if (step.slots.empty()) break;
        print_step(iter, step);
        printf("\n");
    }

    // key observations printed:
    // - no padding. every token slot does useful work.
    // - s0 finishes prefill first, starts decoding while s1 is still prefilling.
    // - when s0 finishes entirely, s2 (or a new request) takes its KV slot.
    // - prefill and decode tokens from different seqs share the same GPU step.

    return 0;
}
