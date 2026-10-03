#pragma once
#include "../../include/ticketing/ICardRepository.h"
#include <unordered_map>

// In-memory stand-in for ICardRepository. Lets Turnstile/FarePolicy tests
// run without touching SQLite at all - exactly the payoff the Phase 3
// repository pattern was built for.
class FakeCardRepository : public ICardRepository {
private:
    std::unordered_map<std::string, CardRecord> cards;

public:
    bool exists(const std::string& cardId) override {
        return cards.find(cardId) != cards.end();
    }

    void create(const std::string& cardId, CardType type, double initialBalance) override {
        CardRecord rec;
        rec.cardId = cardId;
        rec.cardType = type;
        rec.balance = initialBalance;
        rec.entryStationId = -1;
        rec.entryTime = "";
        cards[cardId] = rec;
    }

    bool find(const std::string& cardId, CardRecord& out) override {
        auto it = cards.find(cardId);
        if (it == cards.end()) return false;
        out = it->second;
        return true;
    }

    void recharge(const std::string& cardId, double amount) override {
        auto it = cards.find(cardId);
        if (it != cards.end()) it->second.balance += amount;
    }

    void setEntryStation(const std::string& cardId, int stationId) override {
        auto it = cards.find(cardId);
        if (it == cards.end()) return;
        if (stationId == -1) {
            it->second.entryStationId = -1;
            it->second.entryTime = "";
        } else {
            it->second.entryStationId = stationId;
            it->second.entryTime = "2026-01-01T00:00:00Z"; // fixed value, irrelevant to unit-level checks
        }
    }

    // Test helper, not part of ICardRepository - deducts balance directly
    // so tests can simulate what a real checkout transaction would leave behind.
    void setBalance(const std::string& cardId, double balance) {
        auto it = cards.find(cardId);
        if (it != cards.end()) it->second.balance = balance;
    }
};