#include "../../include/ticketing/FarePolicy.h"

double RegularFarePolicy::computeFare(double distanceKm, IFareRepository& fareRepo) const {
    return fareRepo.getFareForDistance(distanceKm);
}
std::string RegularFarePolicy::getName() const { return "Regular"; }

double StudentFarePolicy::computeFare(double distanceKm, IFareRepository& fareRepo) const {
    return fareRepo.getFareForDistance(distanceKm) * 0.5; // 50% student concession
}
std::string StudentFarePolicy::getName() const { return "Student"; }

double SeniorCitizenFarePolicy::computeFare(double distanceKm, IFareRepository& fareRepo) const {
    return fareRepo.getFareForDistance(distanceKm) * 0.5; // 50% senior citizen concession
}
std::string SeniorCitizenFarePolicy::getName() const { return "Senior Citizen"; }

double DailyPassFarePolicy::computeFare(double distanceKm, IFareRepository& fareRepo) const {
    (void)distanceKm;
    (void)fareRepo; // Flat rate regardless of distance - see header note on scope.
    return 40.0;
}
std::string DailyPassFarePolicy::getName() const { return "Daily Pass"; }