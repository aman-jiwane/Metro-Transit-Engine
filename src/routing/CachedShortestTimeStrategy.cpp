#include "../../include/routing/CachedShortestTimeStrategy.h"

CachedShortestTimeStrategy::CachedShortestTimeStrategy(size_t capacity) : cache(capacity) {}

long long CachedShortestTimeStrategy::makeKey(int start, int end) {
    // Station IDs are well under 100,000 on this network, so this encoding
    // can never collide - simpler than writing a custom hash for std::pair.
    return static_cast<long long>(start) * 100000 + end;
}

RouteResult CachedShortestTimeStrategy::findPath(const MetroNetwork& network, const DelayService& delayService, int start, int end) {
    long long key = makeKey(start, end);

    RouteResult cached;
    if (cache.get(key, cached)) {
        return cached;
    }

    RouteResult result = inner.findPath(network, delayService, start, end);
    cache.put(key, result);
    return result;
}

void CachedShortestTimeStrategy::onIncidentReported(const DelayEvent& event) {
    (void)event;
    cache.clear();
}

void CachedShortestTimeStrategy::invalidateCache() {
    cache.clear();
}