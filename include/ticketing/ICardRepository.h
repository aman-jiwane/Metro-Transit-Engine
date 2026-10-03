#pragma once
#include <string>
#include "../models/CardType.h"

struct CardRecord {
    std::string cardId;
    CardType cardType;
    double balance;
    int entryStationId;   // -1 means "not checked in" (NULL in the DB)
    std::string entryTime; // empty means "not checked in" (NULL in the DB)
};

class ICardRepository {
public:
    virtual ~ICardRepository() = default;

    virtual bool exists(const std::string& cardId) = 0;
    virtual void create(const std::string& cardId, CardType type, double initialBalance) = 0;
    virtual bool find(const std::string& cardId, CardRecord& out) = 0;

    // Increments balance. The audit trigger fires automatically on this
    // UPDATE and writes the audit_log row - no separate logging call needed.
    virtual void recharge(const std::string& cardId, double amount) = 0;

    // Sets entry_station_id/entry_time together (check-in), or clears both
    // by passing stationId = -1 (check-out).
    virtual void setEntryStation(const std::string& cardId, int stationId) = 0;
};