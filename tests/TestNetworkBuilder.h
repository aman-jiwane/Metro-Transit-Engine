#pragma once
#include "../include/network/MetroNetwork.h"

// A small, fixed network used across tests instead of the full 59-station
// Pune network, so test expectations can be hand-verified:
//
//   Line 1 (id 1): A(0) - B(1) - C(2) - D(3)      [each hop: 2.0 min, 1.0 km]
//   Line 2 (id 2): E(4) - C(2) - F(5)              [each hop: 3.0 min, 1.5 km]
//
// Station C (id 2) is the only interchange, connecting both lines.
inline void buildTestNetwork(MetroNetwork& network) {
    network.addLine(1, "Line1");
    network.addLine(2, "Line2");

    network.addStation(0, "A", false);
    network.addStation(1, "B", false);
    network.addStation(2, "C", true); // interchange
    network.addStation(3, "D", false);
    network.addStation(4, "E", false);
    network.addStation(5, "F", false);

    network.addRoute(0, 1, 2.0, 1.0, 1); // A-B on Line1
    network.addRoute(1, 2, 2.0, 1.0, 1); // B-C on Line1
    network.addRoute(2, 3, 2.0, 1.0, 1); // C-D on Line1

    network.addRoute(4, 2, 3.0, 1.5, 2); // E-C on Line2
    network.addRoute(2, 5, 3.0, 1.5, 2); // C-F on Line2
}