#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../TestNetworkBuilder.h"
#include "../fakes/FakeCardRepository.h"
#include "../fakes/FakeTripRepository.h"
#include "../fakes/FakeFareRepository.h"
#include "../../include/ticketing/Turnstile.h"
#include "../../include/routing/ShortestTime.h"
#include "../../include/network/DelayService.h"

using Catch::Matchers::WithinAbs;

namespace {
// Bundles everything a Turnstile test needs so each TEST_CASE stays short.
struct TurnstileFixture {
    MetroNetwork network;
    DelayService delayService;
    ShortestTimeStrategy router;
    FakeCardRepository cardRepo;
    FakeTripRepository tripRepo;
    FakeFareRepository fareRepo;
    Turnstile gate;

    TurnstileFixture()
        : gate(network, delayService, router, cardRepo, tripRepo, fareRepo) {
        buildTestNetwork(network);
        tripRepo.cardRepo = &cardRepo;
    }
};
}

TEST_CASE("Tap in succeeds with sufficient balance and the gate returns to Locked", "[turnstile]") {
    TurnstileFixture f;
    f.cardRepo.create("U1", CardType::REGULAR, 100.0);

    REQUIRE(f.gate.tapCard("U1", 0, false).success);
    REQUIRE(f.gate.getStateName() == "Locked"); // auto walk-through completed
}

TEST_CASE("Tap in is rejected below the minimum balance", "[turnstile]") {
    TurnstileFixture f;
    f.cardRepo.create("U1", CardType::REGULAR, 5.0); // below the 15 INR minimum

    REQUIRE_FALSE(f.gate.tapCard("U1", 0, false).success);
}

TEST_CASE("A second tap-in is rejected while already checked in (double-tap-in prevention)", "[turnstile]") {
    TurnstileFixture f;
    f.cardRepo.create("U1", CardType::REGULAR, 100.0);

    REQUIRE(f.gate.tapCard("U1", 0, false).success);
    REQUIRE_FALSE(f.gate.tapCard("U1", 1, false).success); // rejected, still checked in at station 0

    CardRecord rec;
    f.cardRepo.find("U1", rec);
    REQUIRE(rec.entryStationId == 0); // unchanged by the rejected second tap
}

TEST_CASE("Tap out with no prior check-in is rejected", "[turnstile]") {
    TurnstileFixture f;
    f.cardRepo.create("U1", CardType::REGULAR, 100.0);

    REQUIRE_FALSE(f.gate.tapCard("U1", 3, true).success);
}

TEST_CASE("A full trip deducts the correct fare and records it", "[turnstile]") {
    TurnstileFixture f;
    f.cardRepo.create("U1", CardType::REGULAR, 100.0);

    REQUIRE(f.gate.tapCard("U1", 0, false).success); // A
    REQUIRE(f.gate.tapCard("U1", 3, true).success);   // D: distance 3.0 km -> fare 15.0 (<=5km slab)

    REQUIRE(f.tripRepo.trips.size() == 1);
    REQUIRE_THAT(f.tripRepo.trips[0].distanceKm, WithinAbs(3.0, 1e-9));
    REQUIRE_THAT(f.tripRepo.trips[0].fareCharged, WithinAbs(15.0, 1e-9));
    REQUIRE(f.tripRepo.trips[0].fareTypeUsed == "Regular");

    CardRecord rec;
    f.cardRepo.find("U1", rec);
    REQUIRE_THAT(rec.balance, WithinAbs(85.0, 1e-9));
    REQUIRE(rec.entryStationId == -1); // cleared after checkout
}

TEST_CASE("Checkout is rejected when the fare exceeds the balance, and nothing is recorded", "[turnstile]") {
    TurnstileFixture f;
    f.cardRepo.create("U1", CardType::REGULAR, 16.0);

    f.cardRepo.setBalance("U1", 16.0);
    REQUIRE(f.gate.tapCard("U1", 0, false).success);
    f.cardRepo.setBalance("U1", 10.0); // simulate balance dropping below the fare between tap-in and tap-out

    REQUIRE_FALSE(f.gate.tapCard("U1", 5, true).success); // A->F costs 15.0, only 10.0 available
    REQUIRE(f.tripRepo.trips.empty());

    CardRecord rec;
    f.cardRepo.find("U1", rec);
    REQUIRE(rec.entryStationId == 0); // still checked in - rejected checkout doesn't clear state
}

TEST_CASE("Out of order blocks taps, and repair restores service", "[turnstile]") {
    TurnstileFixture f;
    f.cardRepo.create("U1", CardType::REGULAR, 100.0);

    f.gate.triggerMaintenance();
    REQUIRE(f.gate.getStateName() == "Out of Order");
    REQUIRE_FALSE(f.gate.tapCard("U1", 0, false).success);

    f.gate.repairGate();
    REQUIRE(f.gate.getStateName() == "Locked");
    REQUIRE(f.gate.tapCard("U1", 0, false).success); // works again
}

TEST_CASE("Checkout fails cleanly when every route to the exit is closed", "[turnstile]") {
    TurnstileFixture f;
    f.cardRepo.create("U1", CardType::REGULAR, 100.0);
    f.delayService.closeTrack(1, 2, 1, IncidentType::MAINTENANCE, 30, "t"); // severs A/B from C/D/E/F

    REQUIRE(f.gate.tapCard("U1", 0, false).success); // tap in at A still works
    REQUIRE_FALSE(f.gate.tapCard("U1", 3, true).success); // D is now unreachable from A
    REQUIRE(f.tripRepo.trips.empty());
}