#pragma once
#include <list>
#include <unordered_map>
#include <cstddef>

// Textbook LRU cache: O(1) get/put via a doubly linked list (most-recently-
// used at the front) plus a hash map from key to that entry's list
// iterator. Used to avoid re-running Dijkstra for repeated (start, end)
// route queries - a handful of station pairs (major interchange hubs) tend
// to dominate real query traffic, which is exactly the access pattern an
// LRU cache is built for.
template <typename Key, typename Value>
class LRUCache {
private:
    struct Entry {
        Key key;
        Value value;
    };
    std::list<Entry> items; // most-recently-used at front, least at back
    std::unordered_map<Key, typename std::list<Entry>::iterator> lookup;
    size_t capacity;

public:
    explicit LRUCache(size_t cap) : capacity(cap) {}

    bool get(const Key& key, Value& outValue) {
        auto it = lookup.find(key);
        if (it == lookup.end()) return false;
        items.splice(items.begin(), items, it->second); // O(1): move to front
        outValue = it->second->value;
        return true;
    }

    void put(const Key& key, const Value& value) {
        auto it = lookup.find(key);
        if (it != lookup.end()) {
            it->second->value = value;
            items.splice(items.begin(), items, it->second);
            return;
        }
        if (capacity > 0 && items.size() >= capacity) {
            lookup.erase(items.back().key); // evict least recently used
            items.pop_back();
        }
        items.push_front(Entry{key, value});
        lookup[key] = items.begin();
    }

    void clear() {
        items.clear();
        lookup.clear();
    }

    size_t size() const { return items.size(); }
};