#include "../../include/ticketing/TurnstileState.h"
#include "../../include/ticketing/Turnstile.h"

// ---------------- LockedState ----------------

TapOutcome LockedState::handleTap(Turnstile& gate, const std::string& cardId, int stationId, bool isExit) {
    TapOutcome outcome = isExit ? gate.performCheckOut(cardId, stationId) : gate.performCheckIn(cardId, stationId);
    if (outcome.success) {
        gate.setState(std::make_unique<UnlockedState>());
    }
    return outcome;
}

StateActionResult LockedState::handlePush(Turnstile&) {
    return { false, "Gate is locked. Please tap your card first." };
}

StateActionResult LockedState::handleMaintenance(Turnstile& gate) {
    gate.setState(std::make_unique<OutOfOrderState>());
    return { true, "Gate taken out of service for maintenance." };
}

StateActionResult LockedState::handleRepair(Turnstile&) {
    return { false, "Gate is already operational." };
}

std::string LockedState::getName() const { return "Locked"; }

// ---------------- UnlockedState ----------------

TapOutcome UnlockedState::handleTap(Turnstile&, const std::string&, int, bool) {
    TapOutcome outcome;
    outcome.success = false;
    outcome.message = "Gate already unlocked - please walk through before tapping again.";
    return outcome;
}

StateActionResult UnlockedState::handlePush(Turnstile& gate) {
    gate.setState(std::make_unique<LockedState>());
    return { true, "Gate pushed open - passing through." };
}

StateActionResult UnlockedState::handleMaintenance(Turnstile& gate) {
    gate.setState(std::make_unique<OutOfOrderState>());
    return { true, "Gate taken out of service for maintenance." };
}

StateActionResult UnlockedState::handleRepair(Turnstile&) {
    return { false, "Gate is already operational." };
}

std::string UnlockedState::getName() const { return "Unlocked"; }

// ---------------- OutOfOrderState ----------------

TapOutcome OutOfOrderState::handleTap(Turnstile&, const std::string&, int, bool) {
    TapOutcome outcome;
    outcome.success = false;
    outcome.message = "Out of order. Please use another gate.";
    return outcome;
}

StateActionResult OutOfOrderState::handlePush(Turnstile&) {
    return { false, "Out of order. Gate will not open." };
}

StateActionResult OutOfOrderState::handleMaintenance(Turnstile&) {
    return { false, "Gate is already out of order." };
}

StateActionResult OutOfOrderState::handleRepair(Turnstile& gate) {
    gate.setState(std::make_unique<LockedState>());
    return { true, "Gate repaired and locked, ready for use." };
}

std::string OutOfOrderState::getName() const { return "Out of Order"; }