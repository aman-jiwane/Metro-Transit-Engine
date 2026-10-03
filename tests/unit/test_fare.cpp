#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../fakes/FakeFareRepository.h"
#include "../../include/ticketing/FareCalculatorFactory.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("RegularFarePolicy charges the base slab fare with no discount", "[fare]") {
    FakeFareRepository fareRepo;
    auto policy = FareCalculatorFactory::createPolicy(CardType::REGULAR);

    REQUIRE_THAT(policy->computeFare(1.0, fareRepo), WithinAbs(10.0, 1e-9));  // <=2km slab
    REQUIRE_THAT(policy->computeFare(12.0, fareRepo), WithinAbs(25.0, 1e-9)); // <=14km slab
    REQUIRE(policy->getName() == "Regular");
}

TEST_CASE("StudentFarePolicy applies exactly a 50% concession", "[fare]") {
    FakeFareRepository fareRepo;
    auto policy = FareCalculatorFactory::createPolicy(CardType::STUDENT);

    REQUIRE_THAT(policy->computeFare(12.0, fareRepo), WithinAbs(12.5, 1e-9)); // 25.0 * 0.5
    REQUIRE(policy->getName() == "Student");
}

TEST_CASE("SeniorCitizenFarePolicy applies exactly a 50% concession", "[fare]") {
    FakeFareRepository fareRepo;
    auto policy = FareCalculatorFactory::createPolicy(CardType::SENIOR_CITIZEN);

    REQUIRE_THAT(policy->computeFare(20.0, fareRepo), WithinAbs(15.0, 1e-9)); // 30.0 * 0.5
    REQUIRE(policy->getName() == "Senior Citizen");
}

TEST_CASE("DailyPassFarePolicy is a flat rate regardless of distance", "[fare]") {
    FakeFareRepository fareRepo;
    auto policy = FareCalculatorFactory::createPolicy(CardType::DAILY_PASS);

    REQUIRE_THAT(policy->computeFare(0.5, fareRepo), WithinAbs(40.0, 1e-9));
    REQUIRE_THAT(policy->computeFare(50.0, fareRepo), WithinAbs(40.0, 1e-9));
    REQUIRE(policy->getName() == "Daily Pass");
}

TEST_CASE("Fare slabs apply threshold boundaries correctly (<=, not <)", "[fare]") {
    FakeFareRepository fareRepo;

    REQUIRE_THAT(fareRepo.getFareForDistance(2.0), WithinAbs(10.0, 1e-9));   // exactly on boundary -> lower slab
    REQUIRE_THAT(fareRepo.getFareForDistance(2.001), WithinAbs(15.0, 1e-9)); // just over -> next slab
    REQUIRE_THAT(fareRepo.getFareForDistance(0.0), WithinAbs(10.0, 1e-9));  // same-station tap
}