#include "grid.hpp"
#include <stdexcept>

namespace cacache {

CAGrid::CAGrid(size_t num_cells)
    : cells_(num_cells), next_(num_cells) {
    if (num_cells == 0)
        throw std::invalid_argument("CAGrid: num_cells must be > 0");
}

Cell& CAGrid::cell(size_t idx) {
    return cells_.at(idx);
}

const Cell& CAGrid::cell(size_t idx) const {
    return cells_.at(idx);
}

void CAGrid::record_hit(size_t idx) {
    auto& c = cells_.at(idx);
    ++c.access_count;
    ++c.hit_count;
    // Exponential moving average for hit_rate
    c.hit_rate = (1.0f - kAlpha) * c.hit_rate + kAlpha * 1.0f;
    if (c.state == CellState::EMPTY) c.state = CellState::COLD;
}

void CAGrid::record_miss(size_t idx) {
    auto& c = cells_.at(idx);
    ++c.access_count;
    c.hit_rate = (1.0f - kAlpha) * c.hit_rate + kAlpha * 0.0f;
    if (c.state == CellState::EMPTY) c.state = CellState::COLD;
}

void CAGrid::record_eviction(size_t idx) {
    auto& c = cells_.at(idx);
    ++c.eviction_count;
    // Pressure decays slightly after an eviction (we just freed space)
    c.pressure = std::max(0.0f, c.pressure - 0.05f);
}

// Determine the next state based on current hit_rate and neighbour pressure
CellState CAGrid::compute_next_state(size_t idx) const {
    const auto& c = cells_[idx];
    float np = neighbour_pressure(idx);

    // A cell becomes HOT either by its own hit rate or by neighbour contagion
    if (c.hit_rate >= kHotThreshold || (c.hit_rate >= kWarmThreshold && np > 0.6f))
        return CellState::HOT;
    if (c.hit_rate >= kWarmThreshold)
        return CellState::WARM;
    if (c.access_count > 0)
        return CellState::COLD;
    return CellState::EMPTY;
}

float CAGrid::neighbour_pressure(size_t idx) const {
    float total = 0.0f;
    int   count = 0;
    if (idx > 0)                  { total += cells_[idx - 1].pressure; ++count; }
    if (idx + 1 < cells_.size()) { total += cells_[idx + 1].pressure; ++count; }
    return count > 0 ? total / count : 0.0f;
}

void CAGrid::step() {
    // Compute next generation into double-buffer
    for (size_t i = 0; i < cells_.size(); ++i) {
        next_[i]         = cells_[i];          // carry over stats
        next_[i].state   = compute_next_state(i);
        // Pressure update: nudge toward load factor (caller must set pressure)
        // No change here — pressure is set externally by the orchestrator
    }
    std::swap(cells_, next_);
}

} // namespace cacache
