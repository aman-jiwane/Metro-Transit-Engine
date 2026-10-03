#pragma once
#include "../../include/ticketing/IFareRepository.h"

// Fixed, deterministic fare table for unit tests - deliberately identical
// to the real Pune Metro seed data (008_seed_fares.sql) so FarePolicy tests
// are checking realistic numbers, but without touching SQLite.
class FakeFareRepository : public IFareRepository {
public:
    double getFareForDistance(double distanceKm) override {
        if (distanceKm <= 2.0) return 10.0;
        if (distanceKm <= 5.0) return 15.0;
        if (distanceKm <= 9.0) return 20.0;
        if (distanceKm <= 14.0) return 25.0;
        if (distanceKm <= 20.0) return 30.0;
        return 35.0;
    }
};