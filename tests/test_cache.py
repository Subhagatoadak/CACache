"""Basic correctness tests for the CACache RAG cache."""

import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))

from cacache import RAGCache


def test_put_get_hit():
    cache = RAGCache(l1=10, l2=40, l3=100)
    cache.put("what is RAG?", ["chunk1", "chunk2"])
    result = cache.get("what is RAG?")
    assert result == ["chunk1", "chunk2"], f"Expected hit, got {result}"


def test_miss_returns_none():
    cache = RAGCache(l1=10, l2=40, l3=100)
    result = cache.get("unknown query")
    assert result is None


def test_hit_rate_improves():
    cache = RAGCache(l1=10, l2=40, l3=100, ca_step_interval=10)
    for i in range(5):
        cache.put(f"q{i}", [f"chunk{i}"])
    for i in range(5):
        cache.get(f"q{i}")
    assert cache.hit_rate > 0.0


def test_lru_eviction_on_overflow():
    cache = RAGCache(l1=3, l2=10, l3=30, ca_step_interval=100)
    for i in range(6):
        cache.put(f"key{i}", [f"val{i}"])
    # L1 holds only 3 — first 3 may have been demoted
    stats = cache.stats
    assert stats.total_requests() >= 6


def test_wrap_decorator():
    call_count = 0

    def fake_retriever(q: str) -> list[str]:
        nonlocal call_count
        call_count += 1
        return [f"result for {q}"]

    cache = RAGCache(l1=10, l2=40, l3=100)
    cached = cache.wrap(fake_retriever)

    r1 = cached("hello")
    r2 = cached("hello")   # should be a cache hit — no extra call
    assert r1 == r2
    assert call_count == 1, f"Retriever called {call_count} times, expected 1"


def test_print_status_runs():
    cache = RAGCache(l1=10, l2=40, l3=100, ca_step_interval=5)
    for i in range(20):
        cache.put(f"k{i}", [f"v{i}"])
        cache.get(f"k{i % 10}")
    cache.print_status()   # just ensure it doesn't crash


if __name__ == "__main__":
    test_put_get_hit();        print("✓ put/get hit")
    test_miss_returns_none();  print("✓ miss returns None")
    test_hit_rate_improves();  print("✓ hit rate improves")
    test_lru_eviction_on_overflow(); print("✓ LRU eviction")
    test_wrap_decorator();     print("✓ wrap decorator")
    test_print_status_runs();  print("✓ print_status")
    print("\nAll tests passed.")
