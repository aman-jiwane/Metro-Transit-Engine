#pragma once
#include "RoutingStrategy.h"

class ShortestTimeStrategy : public RoutingStrategy {
public:
    RouteResult findPath(const MetroNetwork& network, const DelayService& delayService, int start, int end) override;
};