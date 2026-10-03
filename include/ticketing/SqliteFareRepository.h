#pragma once
#include "IFareRepository.h"
#include <vector>
#include <utility>

class Database;

// Loads fare_slabs once at construction (cache-aside - fare tiers are
// static reference data, same reasoning as the network graph) and answers
// getFareForDistance() from the in-memory copy rather than querying SQLite
// on every fare calculation.
class SqliteFareRepository : public IFareRepository {
private:
    std::vector<std::pair<double, double>> slabs; // (max_distance_km, fare), sorted ascending

public:
    explicit SqliteFareRepository(Database& db);
    double getFareForDistance(double distanceKm) override;
};