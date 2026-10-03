#pragma once
#include <string>
#include <memory>
#include "../network/MetroNetwork.h"
#include "../network/DelayService.h"
#include "../routing/RoutingStrategy.h"
#include "ICardRepository.h"
#include "ITripRepository.h"
#include "IFareRepository.h"
#include "TurnstileState.h"
#include "TapOutcome.h"

class Turnstile {
private:
    const MetroNetwork& network;
    const DelayService& delayService;
    // Reference, not owned: main.cpp shares one CachedShortestTimeStrategy
    // between Turnstile's fare-distance lookups and the CLI's own route
    // search, so both benefit from the same cache.
    RoutingStrategy& tripRouter;
    ICardRepository& cardRepo;
    ITripRepository& tripRepo;
    IFareRepository& fareRepo;

    std::unique_ptr<TurnstileState> currentState;

    double computeTripDistance(int startId, int endId) const;

public:
    Turnstile(const MetroNetwork& net, const DelayService& delaySvc, RoutingStrategy& router,
              ICardRepository& cardRepository, ITripRepository& tripRepository, IFareRepository& fareRepository);

    // Public gate actions - each delegates to the current state, which
    // decides whether the action is allowed right now and what happens
    // next. None of these print - the caller (CLI text formatting, JSON
    // API response) decides how to present the result.
    TapOutcome tapCard(const std::string& cardId, int stationId, bool isExit);
    StateActionResult pushGate();
    StateActionResult triggerMaintenance();
    StateActionResult repairGate();
    std::string getStateName() const;

    // --- Internal: the real business logic, invoked by TurnstileState
    // subclasses via the Turnstile& they're handed. Not private/friended -
    // simpler than a friend-class relationship - but callers outside the
    // state classes should go through tapCard(), not these directly, since
    // these skip the "is a tap even allowed right now" check.
    void setState(std::unique_ptr<TurnstileState> newState);
    TapOutcome performCheckIn(const std::string& cardId, int stationId);
    TapOutcome performCheckOut(const std::string& cardId, int exitStationId);
};