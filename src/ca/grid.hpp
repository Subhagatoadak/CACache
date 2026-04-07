#pragma once
#include <cstdint>
#include <vector>

namespace cacache {

// Thermal state of a cache tier cell
enum class CellState : uint8_t {
    EMPTY = 0,  // no data, idle
    COLD  = 1,  // low hit rate, low pressure
    WARM  = 2,  // moderate activity
    HOT   = 3,  // high hit rate or high pressure
};

struct Cell {
    CellState state       = CellState::EMPTY;
    float     hit_rate    = 0.0f;   // rolling hit rate [0, 1]
    float     pressure    = 0.0f;   // eviction pressure [0, 1] (full = 1)
    uint64_t  access_count   = 0;
    uint64_t  hit_count      = 0;
    uint64_t  eviction_count = 0;
};

// 1-D CA grid — one cell per cache tier region.
// Boundary conditions: cells at edges treat out-of-bounds neighbors as COLD/empty.
class CAGrid {
public:
    explicit CAGrid(size_t num_cells);

    Cell&       cell(size_t idx);
    const Cell& cell(size_t idx) const;
    size_t      size() const { return cells_.size(); }

    // Record a cache hit/miss/eviction for a cell — updates rolling stats
    void record_hit(size_t idx);
    void record_miss(size_t idx);
    void record_eviction(size_t idx);

    // Advance CA by one generation — recomputes state for all cells
    void step();

private:
    std::vector<Cell> cells_;
    std::vector<Cell> next_;  // double-buffer for synchronous update

    // Thresholds for state transitions
    static constexpr float kHotThreshold  = 0.70f;
    static constexpr float kWarmThreshold = 0.40f;
    static constexpr float kAlpha         = 0.1f;  // EMA smoothing for hit_rate

    CellState compute_next_state(size_t idx) const;
    float     neighbour_pressure(size_t idx) const;  // average pressure of neighbours
};

} // namespace cacache
