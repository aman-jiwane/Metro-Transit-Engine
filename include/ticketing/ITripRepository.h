#pragma once
#include <string>

struct TripOutcome {
    bool success;
    std::string failureReason; // only meaningful when success == false
    double newBalance;         // only meaningful when success == true
};

class ITripRepository {
public:
    virtual ~ITripRepository() = default;

    // Atomically: deducts fareCharged from the card's balance, inserts the
    // trip row, links it back to the audit_log row the balance-change
    // trigger created, and clears the card's check-in state - all inside a
    // single transaction. Rolls back everything, including the balance
    // deduction, if the balance turns out to be insufficient.
    //
    // Returns success=false with a reason instead of throwing for
    // insufficient-balance, since that's an expected, recoverable outcome
    // the caller needs to handle gracefully rather than treating as an
    // exceptional failure.
    virtual TripOutcome recordTrip(
        const std::string& cardId,
        int entryStationId,
        const std::string& entryTime,
        int exitStationId,
        double distanceKm,
        const std::string& fareTypeUsed,
        double fareCharged
    ) = 0;
};