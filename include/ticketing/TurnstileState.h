#pragma once
#include <string>
#include "TapOutcome.h"

class Turnstile; // Context - see Turnstile.h. Forward-declared to avoid a
                  // circular include; only Turnstile& is used here.

// Lightweight result for the non-tap actions (push/maintenance/repair),
// which don't need TapOutcome's checkout-specific fields.
struct StateActionResult {
    bool success;
    std::string message;
};

// Each concrete state decides both how to handle a request AND what the
// next state is - the classic GoF shape, rather than Turnstile itself
// branching on a status enum. None of these print anything themselves
// anymore (Phase 4 had them call std::cout directly) - they return data,
// and the caller (CLI or the future API layer) decides how to present it.
class TurnstileState {
public:
    virtual ~TurnstileState() = default;

    virtual TapOutcome handleTap(Turnstile& gate, const std::string& cardId, int stationId, bool isExit) = 0;
    virtual StateActionResult handlePush(Turnstile& gate) = 0;
    virtual StateActionResult handleMaintenance(Turnstile& gate) = 0;
    virtual StateActionResult handleRepair(Turnstile& gate) = 0;
    virtual std::string getName() const = 0;
};

class LockedState : public TurnstileState {
public:
    TapOutcome handleTap(Turnstile& gate, const std::string& cardId, int stationId, bool isExit) override;
    StateActionResult handlePush(Turnstile& gate) override;
    StateActionResult handleMaintenance(Turnstile& gate) override;
    StateActionResult handleRepair(Turnstile& gate) override;
    std::string getName() const override;
};

class UnlockedState : public TurnstileState {
public:
    TapOutcome handleTap(Turnstile& gate, const std::string& cardId, int stationId, bool isExit) override;
    StateActionResult handlePush(Turnstile& gate) override;
    StateActionResult handleMaintenance(Turnstile& gate) override;
    StateActionResult handleRepair(Turnstile& gate) override;
    std::string getName() const override;
};

class OutOfOrderState : public TurnstileState {
public:
    TapOutcome handleTap(Turnstile& gate, const std::string& cardId, int stationId, bool isExit) override;
    StateActionResult handlePush(Turnstile& gate) override;
    StateActionResult handleMaintenance(Turnstile& gate) override;
    StateActionResult handleRepair(Turnstile& gate) override;
    std::string getName() const override;
};