#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <nanobind/stl/optional.h>

#include "cache/orchestrator.hpp"

namespace nb = nanobind;
using namespace cacache;

NB_MODULE(_cacache_ext, m) {
    m.doc() = "CACache — Cellular Automata orchestrated RAG cache (C++ core)";

    // ── CellState enum ──────────────────────────────────────────────────────
    nb::enum_<CellState>(m, "CellState")
        .value("EMPTY", CellState::EMPTY)
        .value("COLD",  CellState::COLD)
        .value("WARM",  CellState::WARM)
        .value("HOT",   CellState::HOT)
        .export_values();

    // ── CacheStats ──────────────────────────────────────────────────────────
    nb::class_<CacheStats>(m, "CacheStats")
        .def_ro("l1_hits",    &CacheStats::l1_hits)
        .def_ro("l2_hits",    &CacheStats::l2_hits)
        .def_ro("l3_hits",    &CacheStats::l3_hits)
        .def_ro("misses",     &CacheStats::misses)
        .def_ro("promotions", &CacheStats::promotions)
        .def_ro("demotions",  &CacheStats::demotions)
        .def_ro("evictions",  &CacheStats::evictions)
        .def_ro("ca_steps",   &CacheStats::ca_steps)
        .def("total_hits",     &CacheStats::total_hits)
        .def("total_requests", &CacheStats::total_requests)
        .def("hit_rate",       &CacheStats::hit_rate)
        .def("__repr__", [](const CacheStats& s) {
            return "<CacheStats hits=" + std::to_string(s.total_hits()) +
                   " misses=" + std::to_string(s.misses) +
                   " hit_rate=" + std::to_string(s.hit_rate()) + ">";
        });

    // ── TierInfo ────────────────────────────────────────────────────────────
    nb::class_<CacheOrchestrator::TierInfo>(m, "TierInfo")
        .def_ro("state",    &CacheOrchestrator::TierInfo::state)
        .def_ro("hit_rate", &CacheOrchestrator::TierInfo::hit_rate)
        .def_ro("pressure", &CacheOrchestrator::TierInfo::pressure)
        .def_ro("size",     &CacheOrchestrator::TierInfo::size)
        .def_ro("capacity", &CacheOrchestrator::TierInfo::capacity)
        .def("__repr__", [](const CacheOrchestrator::TierInfo& t) {
            return "<TierInfo size=" + std::to_string(t.size) +
                   "/" + std::to_string(t.capacity) +
                   " hit_rate=" + std::to_string(t.hit_rate) +
                   " pressure=" + std::to_string(t.pressure) + ">";
        });

    // ── CacheOrchestrator ───────────────────────────────────────────────────
    nb::class_<CacheOrchestrator>(m, "CacheOrchestrator")
        .def(nb::init<size_t, size_t, size_t, size_t>(),
             nb::arg("l1_capacity"),
             nb::arg("l2_capacity"),
             nb::arg("l3_capacity"),
             nb::arg("ca_step_interval") = 50,
             "Create a 3-tier CA-orchestrated cache.\n\n"
             "Args:\n"
             "    l1_capacity: max entries in L1 (fastest, smallest)\n"
             "    l2_capacity: max entries in L2\n"
             "    l3_capacity: max entries in L3 (slowest, largest)\n"
             "    ca_step_interval: run CA step every N accesses")
        .def("get", &CacheOrchestrator::get,
             nb::arg("key"),
             "Look up key. Returns list of chunks on hit, None on miss.")
        .def("put", &CacheOrchestrator::put,
             nb::arg("key"), nb::arg("chunks"),
             "Insert key → chunks into L1.")
        .def("stats",     &CacheOrchestrator::stats)
        .def("tier_info", &CacheOrchestrator::tier_info, nb::arg("tier_idx"),
             "Inspect CA state of a tier (0=L1, 1=L2, 2=L3).");
}
