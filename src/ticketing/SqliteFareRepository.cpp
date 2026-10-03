#include "../../include/ticketing/SqliteFareRepository.h"
#include "../../include/db/Database.h"
#include "../../include/db/Statement.h"
#include <stdexcept>

SqliteFareRepository::SqliteFareRepository(Database& db) {
    Statement stmt(db, "SELECT max_distance_km, fare FROM fare_slabs ORDER BY max_distance_km ASC;");
    while (stmt.step()) {
        slabs.emplace_back(stmt.columnDouble(0), stmt.columnDouble(1));
    }
    if (slabs.empty()) {
        throw std::runtime_error("fare_slabs table is empty - check that migration 008_seed_fares.sql ran.");
    }
}

double SqliteFareRepository::getFareForDistance(double distanceKm) {
    // First slab whose max_distance_km >= distanceKm. Slabs are sorted
    // ascending by max_distance_km, so this is exactly equivalent to the
    // original if/else-if chain (first threshold met wins).
    for (const auto& slab : slabs) {
        if (distanceKm <= slab.first) {
            return slab.second;
        }
    }
    return slabs.back().second; // Defensive fallback beyond the last slab's ceiling
}