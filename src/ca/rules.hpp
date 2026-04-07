#pragma once
#include "grid.hpp"
#include <vector>

namespace cacache {

enum class Action : uint8_t {
    NONE,
    PROMOTE,   // move LRU entry from this tier UP to the tier above (faster)
    DEMOTE,    // push LRU entry from this tier DOWN to the tier below (slower)
    EVICT,     // discard LRU entry from this tier entirely
    PREFETCH,  // signal that this tier should warm up from its slower neighbour
};

struct RuleOutput {
    size_t cell_idx;
    Action action;
};

// Stateless rule evaluator — reads grid snapshot, emits ordered action list.
// Rules (evaluated per cell, highest-priority first):
//   1. HOT  + pressure > kHighPressure  → DEMOTE  (spill overflow to slower tier)
//   2. HOT  + pressure < kLowPressure   → PROMOTE (pull more from slower tier)
//   3. WARM + neighbour is HOT          → PREFETCH
//   4. COLD + pressure > kHighPressure  → EVICT
//   5. otherwise                        → NONE
class CARules {
public:
    std::vector<RuleOutput> evaluate(const CAGrid& grid) const;

private:
    static constexpr float kHighPressure = 0.80f;
    static constexpr float kLowPressure  = 0.30f;

    Action evaluate_cell(const CAGrid& grid, size_t idx) const;
    bool   has_hot_neighbour(const CAGrid& grid, size_t idx) const;
};

} // namespace cacache
