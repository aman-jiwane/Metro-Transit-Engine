#pragma once
#include <vector>

class MetroNetwork;
class DelayService;

struct RouteResult {
    std::vector<int> path;
    double totalMetric;

    RouteResult() : totalMetric(0.0) {}
};

class RoutingStrategy {
public:
    virtual ~RoutingStrategy() = default;

    virtual RouteResult findPath(const MetroNetwork& network, const DelayService& delayService, int start, int end) = 0;
};