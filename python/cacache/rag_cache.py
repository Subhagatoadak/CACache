"""
RAGCache — high-level Python wrapper around the C++ CacheOrchestrator.

Designed to be dropped into any RAG pipeline as a transparent caching layer
between the retriever and the LLM.  The CA engine runs entirely in C++ and
is invisible to the caller.

Usage
-----
    from cacache import RAGCache

    cache = RAGCache(l1=128, l2=512, l3=2048)

    # In your retrieval function:
    def retrieve(query: str) -> list[str]:
        hit = cache.get(query)
        if hit is not None:
            return hit
        chunks = your_vector_db.search(query)   # expensive call
        cache.put(query, chunks)
        return chunks

    # Inspect CA state:
    cache.print_status()
"""

from __future__ import annotations

from typing import Callable, Optional

from cacache._cacache_ext import CacheOrchestrator, CellState


_STATE_ICON = {
    CellState.EMPTY: "○",
    CellState.COLD:  "❄",
    CellState.WARM:  "~",
    CellState.HOT:   "🔥",
}


class RAGCache:
    """Three-tier CA-orchestrated cache for RAG retrieved chunks.

    Parameters
    ----------
    l1 : int
        L1 (fastest) tier capacity in number of entries. Keep small.
    l2 : int
        L2 tier capacity.
    l3 : int
        L3 (largest) tier capacity.
    ca_step_interval : int
        How often (in accesses) to run a CA step and apply orchestration.
    """

    def __init__(
        self,
        l1: int = 64,
        l2: int = 256,
        l3: int = 1024,
        ca_step_interval: int = 50,
    ) -> None:
        self._cache = CacheOrchestrator(l1, l2, l3, ca_step_interval)

    # ── Core interface ────────────────────────────────────────────────────────

    def get(self, query: str) -> Optional[list[str]]:
        """Return cached chunks for *query*, or ``None`` on a miss."""
        return self._cache.get(query)

    def put(self, query: str, chunks: list[str]) -> None:
        """Store *chunks* for *query* in L1."""
        self._cache.put(query, chunks)

    def wrap(self, retriever: Callable[[str], list[str]]) -> Callable[[str], list[str]]:
        """Decorator / wrapper that adds caching to an existing retriever function.

        Example::

            cached_retrieve = cache.wrap(vector_db.search)
            chunks = cached_retrieve("what is RAG?")
        """
        def _wrapped(query: str) -> list[str]:
            hit = self.get(query)
            if hit is not None:
                return hit
            result = retriever(query)
            self.put(query, result)
            return result

        _wrapped.__name__ = f"cached_{getattr(retriever, '__name__', 'retriever')}"
        return _wrapped

    # ── Stats & inspection ────────────────────────────────────────────────────

    @property
    def hit_rate(self) -> float:
        """Overall cache hit rate across all tiers."""
        return self._cache.stats().hit_rate()

    @property
    def stats(self):
        return self._cache.stats()

    def print_status(self) -> None:
        """Print a human-readable snapshot of cache tier states."""
        s = self._cache.stats()
        print(f"CACache  hit_rate={s.hit_rate():.1%}  "
              f"hits={s.total_hits()}  misses={s.misses}  "
              f"promotions={s.promotions}  demotions={s.demotions}  "
              f"ca_steps={s.ca_steps}")
        for i, name in enumerate(["L1", "L2", "L3"]):
            t = self._cache.tier_info(i)
            icon = _STATE_ICON.get(t.state, "?")
            bar_len = 20
            filled = int(t.pressure * bar_len)
            bar = "█" * filled + "░" * (bar_len - filled)
            print(f"  {name} {icon}  [{bar}] {t.size}/{t.capacity}"
                  f"  hit_rate={t.hit_rate:.2f}  pressure={t.pressure:.2f}")
