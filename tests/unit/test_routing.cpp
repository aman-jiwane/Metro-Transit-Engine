#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../TestNetworkBuilder.h"
#include "../../include/routing/ShortestTime.h"
#include "../../include/routing/FewestStations.h"
#include "../../include/routing/CachedShortestTimeStrategy.h"
#include "../../include/network/DelayService.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("ShortestTimeStrategy finds the direct path with no interchange penalty", "[routing]") {
    MetroNetwork network;
    buildTestNetwork(network);
    DelayService delayService;
    ShortestTimeStrategy strategy;

    RouteResult result = strategy.findPath(network, delayService, 0, 3); // A -> D, all on Line1

    REQUIRE(result.path == std::vector<int>{0, 1, 2, 3});
    REQUIRE_THAT(result.totalMetric, WithinAbs(6.0, 1e-9)); // 2.0 + 2.0 + 2.0, no transfer
}

TEST_CASE("ShortestTimeStrategy adds the interchange penalty when the path changes lines", "[routing]") {
    MetroNetwork network;
    buildTestNetwork(network);
    DelayService delayService;
    ShortestTimeStrategy strategy;

    RouteResult result = strategy.findPath(network, delayService, 0, 5); // A -> F: Line1 then Line2 at C

    REQUIRE(result.path == std::vector<int>{0, 1, 2, 5});
    // 2.0 (A-B) + 2.0 (B-C) + 3.0 (C-F) + 5.0 interchange penalty = 12.0
    REQUIRE_THAT(result.totalMetric, WithinAbs(12.0, 1e-9));
}

TEST_CASE("ShortestTimeStrategy incorporates an active delay", "[routing]") {
    MetroNetwork network;
    buildTestNetwork(network);
    DelayService delayService;
    ShortestTimeStrategy strategy;

    delayService.reportIncident(1, 2, 1, IncidentType::SIGNAL_FAILURE, Severity::MAJOR, 30, "test"); // B-C, +10 min

    RouteResult result = strategy.findPath(network, delayService, 0, 3); // A -> D
    REQUIRE_THAT(result.totalMetric, WithinAbs(16.0, 1e-9)); // 6.0 base + 10.0 delay
}

TEST_CASE("ShortestTimeStrategy routes around a closed track", "[routing]") {
    MetroNetwork network;
    buildTestNetwork(network);
    DelayService delayService;
    ShortestTimeStrategy strategy;

    // Close B-C: this is the ONLY link from A/B's side to the rest of the network in this test graph
    delayService.closeTrack(1, 2, 1, IncidentType::MAINTENANCE, 30, "test");

    RouteResult result = strategy.findPath(network, delayService, 0, 3); // A -> D, unreachable now
    REQUIRE(result.path.empty());
}

TEST_CASE("FewestStationsStrategy counts stops, not interchanges", "[routing]") {
    MetroNetwork network;
    buildTestNetwork(network);
    DelayService delayService;
    FewestStationsStrategy strategy;

    RouteResult result = strategy.findPath(network, delayService, 0, 5); // A -> F
    REQUIRE(result.path == std::vector<int>{0, 1, 2, 5});
    REQUIRE(result.totalMetric == 3); // 3 hops, regardless of the line change
}

TEST_CASE("FewestStationsStrategy also respects closed tracks", "[routing]") {
    MetroNetwork network;
    buildTestNetwork(network);
    DelayService delayService;
    FewestStationsStrategy strategy;

    delayService.closeTrack(1, 2, 1, IncidentType::MAINTENANCE, 30, "test");
    RouteResult result = strategy.findPath(network, delayService, 0, 3);
    REQUIRE(result.path.empty());
}

TEST_CASE("CachedShortestTimeStrategy returns the same result as the uncached strategy", "[routing][cache]") {
    MetroNetwork network;
    buildTestNetwork(network);
    DelayService delayService;
    CachedShortestTimeStrategy cached;

    RouteResult first = cached.findPath(network, delayService, 0, 5);
    RouteResult second = cached.findPath(network, delayService, 0, 5); // should be a cache hit

    REQUIRE(first.path == second.path);
    REQUIRE_THAT(first.totalMetric, WithinAbs(second.totalMetric, 1e-9));
    REQUIRE_THAT(second.totalMetric, WithinAbs(12.0, 1e-9));
}

TEST_CASE("CachedShortestTimeStrategy invalidates on a new incident and returns an updated result", "[routing][cache]") {
    MetroNetwork network;
    buildTestNetwork(network);
    DelayService delayService;
    CachedShortestTimeStrategy cached;
    delayService.subscribeGlobal(&cached);

    RouteResult before = cached.findPath(network, delayService, 0, 3);
    REQUIRE_THAT(before.totalMetric, WithinAbs(6.0, 1e-9));

    delayService.reportIncident(1, 2, 1, IncidentType::SIGNAL_FAILURE, Severity::MAJOR, 30, "test"); // notifies cached

    RouteResult after = cached.findPath(network, delayService, 0, 3);
    REQUIRE_THAT(after.totalMetric, WithinAbs(16.0, 1e-9)); // must NOT be a stale 6.0
}

TEST_CASE("CachedShortestTimeStrategy invalidateCache() clears stale entries manually", "[routing][cache]") {
    MetroNetwork network;
    buildTestNetwork(network);
    DelayService delayService;
    CachedShortestTimeStrategy cached;

    cached.findPath(network, delayService, 0, 3); // populates cache with 6.0

    delayService.closeTrack(1, 2, 1, IncidentType::MAINTENANCE, 30, "test");
    cached.invalidateCache(); // simulates what main.cpp does after removeIncident/reopenTrack

    RouteResult after = cached.findPath(network, delayService, 0, 3);
    REQUIRE(after.path.empty()); // must reflect the closed track, not the stale cached path
}