#pragma once
#include "RoutingStrategy.h"

class FewestStationsStrategy : public RoutingStrategy {
public:
    RouteResult findPath(const MetroNetwork& network, const DelayService& delayService, int start, int end) override;
};