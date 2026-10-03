#pragma once
#include "ITripRepository.h"

class Database;

class SqliteTripRepository : public ITripRepository {
private:
    Database& db;

public:
    explicit SqliteTripRepository(Database& database);

    TripOutcome recordTrip(
        const std::string& cardId,
        int entryStationId,
        const std::string& entryTime,
        int exitStationId,
        double distanceKm,
        const std::string& fareTypeUsed,
        double fareCharged
    ) override;
};