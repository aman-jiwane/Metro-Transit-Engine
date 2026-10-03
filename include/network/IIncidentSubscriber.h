#pragma once
#include "../models/DelayEvent.h"

// Observer interface. DelayService (the Subject) calls onIncidentReported on
// every registered subscriber whenever a new incident is reported (this
// includes track closures, since closeTrack() reports an incident under the
// hood). Used for two distinct purposes in this project: commuters
// subscribed to a specific line (CardIncidentSubscriber), and the route
// cache invalidating itself on any change anywhere (CachedShortestTimeStrategy).
class IIncidentSubscriber {
public:
    virtual ~IIncidentSubscriber() = default;
    virtual void onIncidentReported(const DelayEvent& event) = 0;
};