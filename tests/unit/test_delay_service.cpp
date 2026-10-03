#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../../include/network/DelayService.h"
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {
// Test spy: records every notification it receives instead of printing.
class SpySubscriber : public IIncidentSubscriber {
public:
    std::vector<DelayEvent> received;
    void onIncidentReported(const DelayEvent& event) override {
        received.push_back(event);
    }
};
}

TEST_CASE("Severity maps to the correct fixed delay", "[delay]") {
    DelayService svc;
    REQUIRE_THAT(svc.getDelay(0, 0), WithinAbs(0.0, 1e-9)); // no incident yet

    svc.reportIncident(0, 1, 1, IncidentType::MAINTENANCE, Severity::MINOR, 30, "t");
    REQUIRE_THAT(svc.getDelay(0, 1), WithinAbs(2.0, 1e-9));

    svc.reportIncident(2, 3, 1, IncidentType::MAINTENANCE, Severity::CRITICAL, 30, "t");
    REQUIRE_THAT(svc.getDelay(2, 3), WithinAbs(20.0, 1e-9));
}

TEST_CASE("getDelay and isTrackClosed are direction-independent", "[delay]") {
    DelayService svc;
    svc.reportIncident(5, 9, 1, IncidentType::WEATHER, Severity::MODERATE, 30, "t");

    REQUIRE_THAT(svc.getDelay(5, 9), WithinAbs(5.0, 1e-9));
    REQUIRE_THAT(svc.getDelay(9, 5), WithinAbs(5.0, 1e-9)); // reversed pair, same track
}

TEST_CASE("closeTrack sets track_closed and a zero delay, distinct from a MAJOR incident", "[delay]") {
    DelayService svc;
    svc.closeTrack(0, 1, 1, IncidentType::MAINTENANCE, 30, "t");

    REQUIRE(svc.isTrackClosed(0, 1));
    REQUIRE_THAT(svc.getDelay(0, 1), WithinAbs(0.0, 1e-9)); // closed tracks aren't counted as a "delay"
}

TEST_CASE("Multiple active incidents on the same track stack additively", "[delay]") {
    DelayService svc;
    svc.reportIncident(0, 1, 1, IncidentType::MAINTENANCE, Severity::MINOR, 30, "a");   // +2
    svc.reportIncident(0, 1, 1, IncidentType::HEAVY_LOAD, Severity::MODERATE, 30, "b"); // +5

    REQUIRE_THAT(svc.getDelay(0, 1), WithinAbs(7.0, 1e-9));
}

TEST_CASE("removeIncident deactivates a specific incident and stops it counting", "[delay]") {
    DelayService svc;
    int id = svc.reportIncident(0, 1, 1, IncidentType::MAINTENANCE, Severity::MAJOR, 30, "t");
    REQUIRE_THAT(svc.getDelay(0, 1), WithinAbs(10.0, 1e-9));

    REQUIRE(svc.removeIncident(id));
    REQUIRE_THAT(svc.getDelay(0, 1), WithinAbs(0.0, 1e-9));

    REQUIRE_FALSE(svc.removeIncident(id)); // already inactive - second removal fails
    REQUIRE_FALSE(svc.removeIncident(9999)); // never existed
}

TEST_CASE("reopenTrack only clears the matching closed track, not unrelated incidents", "[delay]") {
    DelayService svc;
    svc.closeTrack(0, 1, 1, IncidentType::MAINTENANCE, 30, "t");
    svc.reportIncident(2, 3, 1, IncidentType::WEATHER, Severity::MINOR, 30, "t");

    svc.reopenTrack(0, 1);

    REQUIRE_FALSE(svc.isTrackClosed(0, 1));
    REQUIRE_THAT(svc.getDelay(2, 3), WithinAbs(2.0, 1e-9)); // unaffected
}

TEST_CASE("A line subscriber is notified only for its own line", "[delay][observer]") {
    DelayService svc;
    SpySubscriber lineOneSub;
    SpySubscriber lineTwoSub;
    svc.subscribeToLine(1, &lineOneSub);
    svc.subscribeToLine(2, &lineTwoSub);

    svc.reportIncident(0, 1, 1, IncidentType::MAINTENANCE, Severity::MINOR, 30, "t"); // line 1

    REQUIRE(lineOneSub.received.size() == 1);
    REQUIRE(lineTwoSub.received.empty());
    REQUIRE(lineOneSub.received[0].fromStation == 0);
    REQUIRE(lineOneSub.received[0].toStation == 1);
}

TEST_CASE("A global subscriber is notified regardless of line", "[delay][observer]") {
    DelayService svc;
    SpySubscriber globalSub;
    svc.subscribeGlobal(&globalSub);

    svc.reportIncident(0, 1, 1, IncidentType::MAINTENANCE, Severity::MINOR, 30, "t");
    svc.reportIncident(9, 8, 7, IncidentType::WEATHER, Severity::MAJOR, 30, "t"); // different line entirely

    REQUIRE(globalSub.received.size() == 2);
}

TEST_CASE("unsubscribeFromLine stops further notifications", "[delay][observer]") {
    DelayService svc;
    SpySubscriber sub;
    svc.subscribeToLine(1, &sub);
    svc.reportIncident(0, 1, 1, IncidentType::MAINTENANCE, Severity::MINOR, 30, "t");
    REQUIRE(sub.received.size() == 1);

    svc.unsubscribeFromLine(1, &sub);
    svc.reportIncident(0, 1, 1, IncidentType::MAINTENANCE, Severity::MINOR, 30, "t");
    REQUIRE(sub.received.size() == 1); // unchanged
}

TEST_CASE("closeTrack notifies subscribers just like reportIncident", "[delay][observer]") {
    DelayService svc;
    SpySubscriber sub;
    svc.subscribeToLine(1, &sub);

    svc.closeTrack(0, 1, 1, IncidentType::MAINTENANCE, 30, "t");

    REQUIRE(sub.received.size() == 1);
    REQUIRE(sub.received[0].trackClosed);
}