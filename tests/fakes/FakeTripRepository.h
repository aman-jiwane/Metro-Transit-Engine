#pragma once
#include "../../include/ticketing/ITripRepository.h"
#include "FakeCardRepository.h"
#include <vector>

// In-memory stand-in for ITripRepository. Mirrors the real
// SqliteTripRepository's contract (check balance, deduct, record, reject if
// insufficient) without a database, and records every attempted trip so
// tests can assert on what was recorded.
class FakeTripRepository : public ITripRepository {
public:
    struct RecordedTrip {
        std::string cardId;
        int entryStationId;
        int exitStationId;
        double distanceKm;
        std::string fareTypeUsed;
        double fareCharged;
    };

    std::vector<RecordedTrip> trips;

    // Not owned - the test wires this to the same FakeCardRepository the
    // Turnstile under test uses, so balance deductions are visible to both.
    FakeCardRepository* cardRepo = nullptr;

    TripOutcome recordTrip(
        const std::string& cardId,
        int entryStationId,
        const std::string& /*entryTime*/,
        int exitStationId,
        double distanceKm,
        const std::string& fareTypeUsed,
        double fareCharged
    ) override {
        CardRecord card;
        if (!cardRepo || !cardRepo->find(cardId, card)) {
            return { false, "Card not found.", 0.0 };
        }
        if (card.balance < fareCharged) {
            return { false, "Insufficient balance.", card.balance };
        }

        double newBalance = card.balance - fareCharged;
        cardRepo->setBalance(cardId, newBalance);
        cardRepo->setEntryStation(cardId, -1);

        trips.push_back({cardId, entryStationId, exitStationId, distanceKm, fareTypeUsed, fareCharged});
        return { true, "", newBalance };
    }
};