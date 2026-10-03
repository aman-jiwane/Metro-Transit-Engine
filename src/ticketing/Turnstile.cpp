#include "../../include/ticketing/Turnstile.h"
#include "../../include/ticketing/FareCalculatorFactory.h"
#include <sstream>
#include <iomanip>

namespace {
std::string formatMoney(double amount) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << amount;
    return oss.str();
}
}

Turnstile::Turnstile(const MetroNetwork& net, const DelayService& delaySvc, RoutingStrategy& router,
                      ICardRepository& cardRepository, ITripRepository& tripRepository, IFareRepository& fareRepository)
    : network(net), delayService(delaySvc), tripRouter(router),
      cardRepo(cardRepository), tripRepo(tripRepository), fareRepo(fareRepository),
      currentState(std::make_unique<LockedState>()) {}

double Turnstile::computeTripDistance(int startId, int endId) const {
    if (startId == endId) {
        return 0.0;
    }

    RouteResult route = tripRouter.findPath(network, delayService, startId, endId);
    if (route.path.empty()) {
        return -1.0; // No valid route (e.g. every connecting track is closed)
    }

    double totalDistance = 0.0;
    for (size_t i = 0; i + 1 < route.path.size(); ++i) {
        int u = route.path[i];
        int v = route.path[i + 1];

        for (const auto& edge : network.getNeighbors(u)) {
            if (edge.to == v) {
                totalDistance += edge.dist;
                break;
            }
        }
    }
    return totalDistance;
}

void Turnstile::setState(std::unique_ptr<TurnstileState> newState) {
    currentState = std::move(newState);
}

TapOutcome Turnstile::tapCard(const std::string& cardId, int stationId, bool isExit) {
    TapOutcome outcome = currentState->handleTap(*this, cardId, stationId, isExit);
    if (outcome.success) {
        // Simulate walking through: the gate briefly unlocked (state
        // transition already happened inside handleTap), now it closes
        // again. Runs through the real UnlockedState::handlePush logic,
        // not a shortcut - this is what actually exercises the
        // Unlocked -> Locked transition on every successful tap.
        currentState->handlePush(*this);
    }
    return outcome;
}

StateActionResult Turnstile::pushGate() {
    return currentState->handlePush(*this);
}

StateActionResult Turnstile::triggerMaintenance() {
    return currentState->handleMaintenance(*this);
}

StateActionResult Turnstile::repairGate() {
    return currentState->handleRepair(*this);
}

std::string Turnstile::getStateName() const {
    return currentState->getName();
}

TapOutcome Turnstile::performCheckIn(const std::string& cardId, int stationId) {
    TapOutcome outcome;

    CardRecord card;
    if (!cardRepo.find(cardId, card)) {
        outcome.message = "Entry Denied. Card '" + cardId + "' not found.";
        return outcome;
    }

    // Double-tap-in prevention: reject a second check-in instead of
    // silently overwriting the first one's entry station.
    if (card.entryStationId != -1) {
        outcome.message = "Entry Denied. Already checked in at " +
            network.getStationName(card.entryStationId) + " - please tap out before tapping in again.";
        return outcome;
    }

    const double MIN_BALANCE = 15.0;
    if (card.balance < MIN_BALANCE) {
        outcome.message = "Entry Denied. Minimum balance of " + formatMoney(MIN_BALANCE) + " INR required.";
        return outcome;
    }

    cardRepo.setEntryStation(cardId, stationId);

    outcome.success = true;
    outcome.entryStationName = network.getStationName(stationId);
    outcome.remainingBalance = card.balance;
    outcome.message = "Entry Granted at " + outcome.entryStationName + ". Balance: " + formatMoney(card.balance) + " INR.";
    return outcome;
}

TapOutcome Turnstile::performCheckOut(const std::string& cardId, int exitStationId) {
    TapOutcome outcome;
    outcome.isCheckout = true;

    CardRecord card;
    if (!cardRepo.find(cardId, card)) {
        outcome.message = "Exit Denied. Card '" + cardId + "' not found.";
        return outcome;
    }

    if (card.entryStationId == -1) {
        outcome.message = "Exit Denied. No check-in record found for Card " + cardId + ". Please see customer service.";
        return outcome;
    }
    outcome.entryStationName = network.getStationName(card.entryStationId);
    outcome.exitStationName = network.getStationName(exitStationId);

    double distance = computeTripDistance(card.entryStationId, exitStationId);
    if (distance < 0.0) {
        outcome.message = "Exit Denied. No valid route exists between " + outcome.entryStationName +
            " and " + outcome.exitStationName + " (a track on every path may be closed).";
        return outcome;
    }

    auto farePolicy = FareCalculatorFactory::createPolicy(card.cardType);
    double fare = farePolicy->computeFare(distance, fareRepo);
    outcome.fareType = farePolicy->getName();
    outcome.fareCharged = fare;

    TripOutcome dbOutcome = tripRepo.recordTrip(
        cardId, card.entryStationId, card.entryTime, exitStationId,
        distance, farePolicy->getName(), fare
    );

    outcome.remainingBalance = dbOutcome.newBalance;

    if (dbOutcome.success) {
        outcome.success = true;
        outcome.message = "Exit Granted at " + outcome.exitStationName + ". Fare Type: " + outcome.fareType +
            ". Fare Deducted: " + formatMoney(fare) + " INR. Remaining Balance: " + formatMoney(dbOutcome.newBalance) + " INR.";
    } else {
        outcome.message = "Exit Denied. " + dbOutcome.failureReason +
            " (Fare would have been " + formatMoney(fare) + " INR, " + outcome.fareType + " rate).";
    }
    return outcome;
}