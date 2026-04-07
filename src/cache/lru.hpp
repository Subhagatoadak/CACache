#pragma once
#include <list>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cacache {

// Intrusive doubly-linked LRU cache.
// O(1) get, put, erase.  Not thread-safe — caller holds mutex.
template <typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}

    // Returns value if present and moves it to MRU position.
    std::optional<V> get(const K& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return std::nullopt;
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    // Insert or update.  Returns true if an LRU entry was silently evicted.
    bool put(const K& key, V value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = std::move(value);
            list_.splice(list_.begin(), list_, it->second);
            return false;
        }
        list_.emplace_front(key, std::move(value));
        map_[key] = list_.begin();
        if (map_.size() > capacity_) {
            evict_lru();
            return true;
        }
        return false;
    }

    bool contains(const K& key) const { return map_.count(key) > 0; }

    // Erase a specific key.  Returns false if not found.
    bool erase(const K& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return false;
        list_.erase(it->second);
        map_.erase(it);
        return true;
    }

    // Peek at the LRU (oldest) key without evicting.
    std::optional<K> lru_key() const {
        if (list_.empty()) return std::nullopt;
        return list_.back().first;
    }

    // Pop and return the LRU entry.
    std::optional<std::pair<K, V>> pop_lru() {
        if (list_.empty()) return std::nullopt;
        auto entry = std::move(list_.back());
        map_.erase(entry.first);
        list_.pop_back();
        return entry;
    }

    size_t size()     const { return map_.size(); }
    size_t capacity() const { return capacity_; }

    // Load factor in [0, 1] — used as CA pressure signal.
    float load_factor() const {
        return capacity_ > 0 ? static_cast<float>(map_.size()) / capacity_ : 0.0f;
    }

    std::vector<K> keys() const {
        std::vector<K> ks;
        ks.reserve(map_.size());
        for (auto& [k, _] : list_) ks.push_back(k);
        return ks;
    }

private:
    size_t capacity_;
    std::list<std::pair<K, V>> list_;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> map_;

    void evict_lru() {
        if (list_.empty()) return;
        map_.erase(list_.back().first);
        list_.pop_back();
    }
};

} // namespace cacache
