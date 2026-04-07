#include "rules.hpp"

namespace cacache {

std::vector<RuleOutput> CARules::evaluate(const CAGrid& grid) const {
    std::vector<RuleOutput> out;
    out.reserve(grid.size());
    for (size_t i = 0; i < grid.size(); ++i) {
        Action a = evaluate_cell(grid, i);
        if (a != Action::NONE)
            out.push_back({i, a});
    }
    return out;
}

Action CARules::evaluate_cell(const CAGrid& grid, size_t idx) const {
    const auto& c = grid.cell(idx);

    switch (c.state) {
    case CellState::HOT:
        if (c.pressure >= kHighPressure) return Action::DEMOTE;
        if (c.pressure <= kLowPressure)  return Action::PROMOTE;
        return Action::NONE;

    case CellState::WARM:
        if (has_hot_neighbour(grid, idx)) return Action::PREFETCH;
        return Action::NONE;

    case CellState::COLD:
        if (c.pressure >= kHighPressure) return Action::EVICT;
        return Action::NONE;

    default:
        return Action::NONE;
    }
}

bool CARules::has_hot_neighbour(const CAGrid& grid, size_t idx) const {
    if (idx > 0 && grid.cell(idx - 1).state == CellState::HOT) return true;
    if (idx + 1 < grid.size() && grid.cell(idx + 1).state == CellState::HOT) return true;
    return false;
}

} // namespace cacache
