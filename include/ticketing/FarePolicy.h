#pragma once
#include <string>
#include "IFareRepository.h"

// One policy per commuter category. Each wraps the base distance fare
// (looked up from fare_slabs via IFareRepository) and applies whatever
// category-specific rule applies (concession, flat rate, etc).
class FarePolicy {
public:
    virtual ~FarePolicy() = default;
    virtual double computeFare(double distanceKm, IFareRepository& fareRepo) const = 0;
    virtual std::string getName() const = 0;
};

class RegularFarePolicy : public FarePolicy {
public:
    double computeFare(double distanceKm, IFareRepository& fareRepo) const override;
    std::string getName() const override;
};

class StudentFarePolicy : public FarePolicy {
public:
    double computeFare(double distanceKm, IFareRepository& fareRepo) const override;
    std::string getName() const override;
};

class SeniorCitizenFarePolicy : public FarePolicy {
public:
    double computeFare(double distanceKm, IFareRepository& fareRepo) const override;
    std::string getName() const override;
};

// NOTE (scope): a real Daily Pass caps total spend across every ride taken
// in a calendar day, which needs persisted trip history to check against.
// That's available now (trips table + ITripRepository), but querying
// "today's spend for this card" and comparing against a cap is a genuine
// new feature, not a wiring change - deliberately left as a flat per-trip
// rate for now rather than half-implementing the cap logic.
class DailyPassFarePolicy : public FarePolicy {
public:
    double computeFare(double distanceKm, IFareRepository& fareRepo) const override;
    std::string getName() const override;
};