#include "orchestrator.hpp"
#include <stdexcept>

namespace cacache {

CacheOrchestrator::CacheOrchestrator(size_t l1_cap, size_t l2_cap, size_t l3_cap,
                                     size_t ca_step_interval)
    : l1_(l1_cap), l2_(l2_cap), l3_(l3_cap),
      grid_(kNumTiers),
      ca_step_interval_(ca_step_interval) {}

// ── Public interface ──────────────────────────────────────────────────────────

std::optional<Chunks> CacheOrchestrator::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mu_);
    ++access_count_;

    // L1 hit
    if (auto v = l1_.get(key)) {
        grid_.record_hit(0);
        ++stats_.l1_hits;
        sync_pressure();
        maybe_step();
        return v;
    }
    // L2 hit — promote to L1
    if (auto v = l2_.get(key)) {
        grid_.record_hit(1);
        ++stats_.l2_hits;
        l2_.erase(key);
        promote_to(1, key);           // move value up; we already have it
        l1_.put(key, *v);
        sync_pressure();
        maybe_step();
        return v;
    }
    // L3 hit — promote to L2
    if (auto v = l3_.get(key)) {
        grid_.record_hit(2);
        ++stats_.l3_hits;
        l3_.erase(key);
        l2_.put(key, *v);
        sync_pressure();
        maybe_step();
        return v;
    }

    // Miss
    grid_.record_miss(0);
    ++stats_.misses;
    sync_pressure();
    maybe_step();
    return std::nullopt;
}

void CacheOrchestrator::put(const std::string& key, Chunks value) {
    std::lock_guard<std::mutex> lock(mu_);
    ++access_count_;

    // Before inserting into L1, if L1 is full cascade its LRU entry down
    // to L2 (and L2's overflow to L3) so lower tiers are actually populated.
    // The CA orchestrator then controls *when* to promote/demote aggressively;
    // this cascade is the baseline data path that keeps all tiers alive.
    if (l1_.load_factor() >= 1.0f) {
        if (auto evicted = l1_.pop_lru()) {
            if (l2_.load_factor() >= 1.0f) {
                if (auto evicted2 = l2_.pop_lru()) {
                    l3_.put(evicted2->first, std::move(evicted2->second));
                    grid_.record_eviction(1);
                }
            }
            l2_.put(evicted->first, std::move(evicted->second));
            grid_.record_eviction(0);
            ++stats_.demotions;
        }
    }

    l1_.put(key, std::move(value));
    sync_pressure();
    maybe_step();
}

CacheOrchestrator::TierInfo CacheOrchestrator::tier_info(size_t idx) const {
    std::lock_guard<std::mutex> lock(mu_);
    if (idx >= kNumTiers)
        throw std::out_of_range("tier_info: idx out of range");
    const auto& c = grid_.cell(idx);
    const auto& lru = (idx == 0) ? l1_ : (idx == 1) ? l2_ : l3_;
    return {c.state, c.hit_rate, c.pressure, lru.size(), lru.capacity()};
}

// ── Private helpers ───────────────────────────────────────────────────────────

LRUCache<std::string, Chunks>& CacheOrchestrator::tier(size_t idx) {
    if (idx == 0) return l1_;
    if (idx == 1) return l2_;
    return l3_;
}

void CacheOrchestrator::sync_pressure() {
    grid_.cell(0).pressure = l1_.load_factor();
    grid_.cell(1).pressure = l2_.load_factor();
    grid_.cell(2).pressure = l3_.load_factor();
}

void CacheOrchestrator::maybe_step() {
    if (access_count_ % ca_step_interval_ != 0) return;
    grid_.step();
    ++stats_.ca_steps;
    auto actions = rules_.evaluate(grid_);
    apply(actions);
}

void CacheOrchestrator::apply(const std::vector<RuleOutput>& actions) {
    for (const auto& ro : actions) {
        switch (ro.action) {
        case Action::DEMOTE:
            demote_lru(ro.cell_idx);
            break;
        case Action::PROMOTE:
            prefetch_into(ro.cell_idx);
            break;
        case Action::EVICT:
            if (auto entry = tier(ro.cell_idx).pop_lru()) {
                grid_.record_eviction(ro.cell_idx);
                ++stats_.evictions;
            }
            break;
        case Action::PREFETCH:
            prefetch_into(ro.cell_idx);
            break;
        default:
            break;
        }
    }
    sync_pressure();
}

void CacheOrchestrator::promote_to(size_t from_tier, const std::string& key) {
    // The actual value insertion is done by the caller after erase.
    (void)from_tier; (void)key;
    ++stats_.promotions;
}

void CacheOrchestrator::demote_lru(size_t from_tier) {
    if (from_tier + 1 >= kNumTiers) return;
    auto entry = tier(from_tier).pop_lru();
    if (!entry) return;
    tier(from_tier + 1).put(entry->first, std::move(entry->second));
    grid_.record_eviction(from_tier);
    ++stats_.demotions;
}

void CacheOrchestrator::prefetch_into(size_t tier_idx) {
    // Pull the MRU entry from the next (slower) tier into this tier.
    if (tier_idx + 1 >= kNumTiers) return;
    auto& slower = tier(tier_idx + 1);
    auto& faster = tier(tier_idx);
    auto lru_k = slower.lru_key();
    if (!lru_k) return;
    if (faster.contains(*lru_k)) return;         // already present
    auto val = slower.get(*lru_k);               // get promotes it in slower
    if (!val) return;
    faster.put(*lru_k, *val);
    ++stats_.promotions;
}

} // namespace cacache
