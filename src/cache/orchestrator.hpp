#pragma once
#include "lru.hpp"
#include "../ca/grid.hpp"
#include "../ca/rules.hpp"
#include <mutex>
#include <string>
#include <vector>

namespace cacache {

// Value type for the RAG cache: a list of retrieved text chunks.
using Chunks = std::vector<std::string>;

struct CacheStats {
    uint64_t l1_hits   = 0;
    uint64_t l2_hits   = 0;
    uint64_t l3_hits   = 0;
    uint64_t misses    = 0;
    uint64_t promotions  = 0;
    uint64_t demotions   = 0;
    uint64_t evictions   = 0;
    uint64_t ca_steps    = 0;

    uint64_t total_hits() const { return l1_hits + l2_hits + l3_hits; }
    uint64_t total_requests() const { return total_hits() + misses; }
    float    hit_rate() const {
        uint64_t total = total_requests();
        return total > 0 ? static_cast<float>(total_hits()) / total : 0.0f;
    }
};

// Three-tier cache (L1=fastest/smallest … L3=slowest/largest).
// The CA grid has exactly 3 cells, one per tier.
// Every `ca_step_interval` accesses, the CA advances one generation and
// emits orchestration actions (promote / demote / evict / prefetch).
class CacheOrchestrator {
public:
    // Capacities are in number of entries.
    CacheOrchestrator(size_t l1_cap, size_t l2_cap, size_t l3_cap,
                      size_t ca_step_interval = 50);

    // Returns chunks if found in any tier; promotes to L1 on hit.
    // Returns empty optional on miss.
    std::optional<Chunks> get(const std::string& key);

    // Insert into L1.  CA may demote/evict as needed.
    void put(const std::string& key, Chunks value);

    const CacheStats& stats() const { return stats_; }

    // Expose CA grid state for inspection / debugging.
    struct TierInfo {
        CellState state;
        float     hit_rate;
        float     pressure;
        size_t    size;
        size_t    capacity;
    };
    TierInfo tier_info(size_t tier_idx) const;

private:
    static constexpr size_t kNumTiers = 3;

    LRUCache<std::string, Chunks> l1_, l2_, l3_;
    CAGrid   grid_;
    CARules  rules_;

    size_t   access_count_      = 0;
    size_t   ca_step_interval_;
    CacheStats stats_;

    mutable std::mutex mu_;

    LRUCache<std::string, Chunks>& tier(size_t idx);

    void sync_pressure();                    // push load factors into grid cells
    void maybe_step();                       // run CA step if interval elapsed
    void apply(const std::vector<RuleOutput>& actions);

    // Tier-to-tier movements
    void promote_to(size_t from_tier, const std::string& key);
    void demote_lru(size_t from_tier);
    void prefetch_into(size_t tier_idx);
};

} // namespace cacache
