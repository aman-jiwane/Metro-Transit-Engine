#pragma once

// Answers "what does a trip of this distance cost", backed by the
// fare_slabs table instead of the hardcoded if/else chain that used to
// live in FareCalculator.cpp.
class IFareRepository {
public:
    virtual ~IFareRepository() = default;
    virtual double getFareForDistance(double distanceKm) = 0;
};