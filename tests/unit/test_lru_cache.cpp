#include <catch2/catch_test_macros.hpp>
#include "../../include/routing/LRUCache.h"

TEST_CASE("Get on an empty cache returns false", "[lru]") {
    LRUCache<int, int> cache(3);
    int value;
    REQUIRE_FALSE(cache.get(1, value));
}

TEST_CASE("Put then get round-trips the value", "[lru]") {
    LRUCache<int, std::string> cache(3);
    cache.put(1, "one");

    std::string value;
    REQUIRE(cache.get(1, value));
    REQUIRE(value == "one");
}

TEST_CASE("Exceeding capacity evicts the least recently used entry", "[lru]") {
    LRUCache<int, int> cache(2);
    cache.put(1, 100);
    cache.put(2, 200);
    cache.put(3, 300); // evicts key 1 (least recently used)

    int value;
    REQUIRE_FALSE(cache.get(1, value));
    REQUIRE(cache.get(2, value));
    REQUIRE(cache.get(3, value));
    REQUIRE(cache.size() == 2);
}

TEST_CASE("Getting an entry marks it as recently used, protecting it from eviction", "[lru]") {
    LRUCache<int, int> cache(2);
    cache.put(1, 100);
    cache.put(2, 200);

    int value;
    cache.get(1, value); // touch key 1, making key 2 the least recently used now

    cache.put(3, 300); // should evict key 2, not key 1

    REQUIRE(cache.get(1, value));
    REQUIRE_FALSE(cache.get(2, value));
    REQUIRE(cache.get(3, value));
}

TEST_CASE("Putting an existing key updates its value and refreshes recency", "[lru]") {
    LRUCache<int, int> cache(2);
    cache.put(1, 100);
    cache.put(2, 200);
    cache.put(1, 999); // update, also refreshes key 1's recency

    int value;
    cache.get(1, value);
    REQUIRE(value == 999);

    cache.put(3, 300); // should evict key 2 (now least recently used), not key 1
    REQUIRE_FALSE(cache.get(2, value));
    REQUIRE(cache.get(1, value));
}

TEST_CASE("clear() empties the cache", "[lru]") {
    LRUCache<int, int> cache(3);
    cache.put(1, 100);
    cache.put(2, 200);
    cache.clear();

    REQUIRE(cache.size() == 0);
    int value;
    REQUIRE_FALSE(cache.get(1, value));
}