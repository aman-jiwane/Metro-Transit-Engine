#pragma once
#include "RoutingStrategy.h"
#include "ShortestTime.h"
#include "LRUCache.h"
#include "../network/IIncidentSubscriber.h"

// Decorator over ShortestTimeStrategy: checks an LRU cache before running
// Dijkstra, and is itself an IIncidentSubscriber so it can invalidate that
// cache automatically whenever DelayService reports a new incident anywhere
// in the network - this only works because it stays alive for the whole
// program run (main.cpp constructs one instance and shares it), unlike a
// RoutingStrategy that gets freshly constructed per query.
class CachedShortestTimeStrategy : public RoutingStrategy, public IIncidentSubscriber {
private:
    ShortestTimeStrategy inner;
    LRUCache<long long, RouteResult> cache;

    static long long makeKey(int start, int end);

public:
    explicit CachedShortestTimeStrategy(size_t capacity = 50);

    RouteResult findPath(const MetroNetwork& network, const DelayService& delayService, int start, int end) override;

    // A reported incident could change the fastest route for any cached
    // pair, not just ones that used the affected edge. Rather than tracking
    // which cached paths actually touch the affected edge, the whole cache
    // is cleared - simpler, always correct, and cheap to recompute on a
    // 59-station network.
    void onIncidentReported(const DelayEvent& event) override;

    // Exposed separately from onIncidentReported because removeIncident()
    // and reopenTrack() also change effective edge weights but don't fit
    // the "a new incident was reported" shape that IIncidentSubscriber
    // models - main.cpp calls this directly after those two admin actions.
    void invalidateCache();
};