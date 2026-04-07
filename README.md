# CACache

**Cellular Automata orchestrated RAG cache** — a 3-tier cache where a CA engine
controls tier promotion, demotion, prefetch, and eviction instead of relying on
a single flat LRU/LFU policy.

```text
User query
    ↓
┌──────────────────────────────────────┐
│  CA Orchestration Layer  (C++)       │
│  cells = tiers · rules = actions    │
│  PROMOTE / DEMOTE / PREFETCH / EVICT │
└──────────┬───────────────────────────┘
           │
    ┌──────▼──────┐
    │ L1  1 ms    │  ← hot queries, small capacity
    │ L2  10 ms   │  ← warm working set
    │ L3  50 ms   │  ← long-tail, large capacity
    └─────────────┘
           │ miss → vector DB / retriever (150 ms)
```

---

## Why CA orchestration?

Flat LRU and LFU treat all cache entries equally and have no concept of
storage tiers. The CA layer provides:

| Advantage | Mechanism |
| --- | --- |
| **5–9× lower hot-query latency** | Hot entries stay anchored in L1 (1 ms) |
| **Scan pollution resistance** | Cascade absorbs one-time entries in L3; L1 stays clean |
| **Hot-set shift recovery** | Old hot entries survive in L2/L3 as a second-chance tier |
| **Capacity efficiency** | Entries get 3 eviction chances instead of 1 |
| **Burst absorption** | L1 absorbs burst; topic stays warm in L2 after burst ends |

---

## Architecture

```text
CACache/
├── src/
│   ├── ca/
│   │   ├── grid.hpp / grid.cpp        ← CA cells (hit_rate, pressure, COLD/WARM/HOT state)
│   │   ├── rules.hpp / rules.cpp      ← rule evaluator → PROMOTE/DEMOTE/PREFETCH/EVICT
│   ├── cache/
│   │   ├── lru.hpp                    ← O(1) LRU (doubly-linked list + hashmap)
│   │   ├── orchestrator.hpp / .cpp    ← 3-tier cache driven by CA actions
│   └── bindings/
│       └── python.cpp                 ← nanobind Python bindings
├── python/cacache/
│   ├── __init__.py
│   └── rag_cache.py                   ← RAGCache Python wrapper
├── examples/
│   ├── cacache_demo.ipynb             ← walkthrough + basic benchmarks
│   └── cacache_advantage.ipynb        ← comprehensive 8-benchmark comparison suite
├── tests/
│   └── test_cache.py
├── CMakeLists.txt
└── pyproject.toml
```

### How the CA works

Every `ca_step_interval` accesses (default 30):

1. **Sync pressure** — each tier's load factor is written into its CA cell
1. **`grid.step()`** — recompute COLD / WARM / HOT state for every cell using EMA hit_rate and neighbour pressure
1. **`rules.evaluate()`** — emit one action per cell:

| Cell state | Condition | Action |
| --- | --- | --- |
| HOT | pressure ≥ 0.80 | DEMOTE — spill LRU to next tier |
| HOT | pressure ≤ 0.30 | PROMOTE — pull from slower tier |
| WARM | neighbour is HOT | PREFETCH — warm up from slower tier |
| COLD | pressure ≥ 0.80 | EVICT — discard LRU entry |

1. **Apply actions** — orchestrator moves entries between tiers accordingly

Between CA steps, the default data path cascades L1 overflows to L2 and L2 overflows to L3, so all tiers are always populated.

---

## Installation

### Prerequisites

- Python ≥ 3.9
- CMake ≥ 3.21
- C++20 compiler (clang / gcc)
- nanobind ≥ 2.0

```bash
pip install scikit-build-core nanobind
pip install -e .
```

### Run the tests

```bash
python tests/test_cache.py
```

---

## Usage

```python
from cacache import RAGCache

# Create a 3-tier cache sized for a RAG pipeline
cache = RAGCache(l1=20, l2=64, l3=256, ca_step_interval=30)

# Option 1: manual get/put
def retrieve(query: str) -> list[str]:
    hit = cache.get(query)
    if hit is not None:
        return hit
    chunks = vector_db.search(query)          # expensive call
    cache.put(query, chunks)
    return chunks

# Option 2: wrap an existing retriever (recommended)
cached_search = cache.wrap(vector_db.search)
chunks = cached_search("what is attention mechanism?")

# Inspect CA state
cache.print_status()
```

### Example output

```text
CACache  hit_rate=78.3%  hits=391  misses=109  promotions=47  demotions=83  ca_steps=42
  L1 🔥  [████████████████████] 20/20  hit_rate=0.82  pressure=1.00
  L2 🔥  [████████████████░░░░] 52/64  hit_rate=0.91  pressure=0.81
  L3 ❄  [███░░░░░░░░░░░░░░░░░] 38/256  hit_rate=0.31  pressure=0.15
```

---

## Benchmarks

Full benchmark suite is in [`examples/cacache_advantage.ipynb`](examples/cacache_advantage.ipynb).

```bash
pip install matplotlib numpy cachetools jupyter
jupyter notebook examples/cacache_advantage.ipynb
```

### Selected results (trimodal Zipf traffic, same total capacity)

| Metric | CACache | Flat LRU | Flat LFU |
| --- | --- | --- | --- |
| Avg response time | **~2 ms** | ~10 ms | ~10 ms |
| p95 latency | **10 ms** | 10 ms | 10 ms |
| Hot-query hit rate (scan workload) | **higher** | degrades | degrades |
| Phase-3 recovery hit rate (shift) | **higher** | lower | lower |
| Uniform random traffic | equal | equal | equal |

> **Latency advantage is structural** — even at equal hit rates, hot queries
> cost 1 ms from L1 vs 10 ms from a flat cache. At 1000 RPM with 60% hot
> traffic that is ~5.4 seconds of retrieval latency saved per minute.

---

## Configuration guide

| Parameter | Guideline |
| --- | --- |
| `l1` | Expected hot-topic size — e.g. one user session ≈ 10–20 queries |
| `l2` | Working set that should survive between topic shifts |
| `l3` | Long-tail queries worth keeping to avoid cold retrieval |
| `ca_step_interval` | Lower = more reactive (more CA overhead); 20–50 is a good range |

---

## Roadmap

- [ ] Per-entry heat tracking (not just per-tier) for finer-grained eviction
- [ ] Semantic key hashing — cache near-duplicate queries via embedding similarity
- [ ] KV cache tier management for LLM inference (attention key-value blocks)
- [ ] Distributed mode — CA cells span cache nodes across machines
- [ ] Prometheus metrics exporter

---

## License

See [LICENSE](LICENSE).
