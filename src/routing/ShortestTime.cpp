#include "../../include/routing/ShortestTime.h"
#include "../../include/network/MetroNetwork.h"
#include "../../include/network/DelayService.h"
#include <queue>
#include <limits>
#include <algorithm>
#include <map>

namespace {
// Real-world transfer cost: walking between platforms, waiting for the next
// train on the new line, etc. Matches the value originally proposed when
// this feature was first scoped out. A single named constant, not a DB
// table - this is one fixed number, not a per-incident business rule like
// delay severity, so a reference table would be pure overhead here.
constexpr double INTERCHANGE_PENALTY_MINUTES = 5.0;

// State-space key. Ordinary Dijkstra on this graph would use just the
// station id as its state, but that can't tell "arrived via the Purple
// line" apart from "arrived via the Orange line" at an interchange like
// Swargate - and that distinction is exactly what an interchange penalty
// needs. Augmenting the state to (station, arrival_line_id) lets the same
// station be reached multiple times, once per line, each carrying its own
// cost. -1 means "the start station, no line committed yet" so leaving the
// very first station never itself counts as a transfer.
using StateKey = std::pair<int, int>;
}

RouteResult ShortestTimeStrategy::findPath(const MetroNetwork& network, const DelayService& delayService, int start, int end) {
    std::map<StateKey, double> time;
    std::map<StateKey, StateKey> parent;

    std::priority_queue<std::pair<double, StateKey>,
                        std::vector<std::pair<double, StateKey>>,
                        std::greater<std::pair<double, StateKey>>> pq;

    StateKey startState = {start, -1};
    time[startState] = 0.0;
    pq.push({0.0, startState});

    bool found = false;
    StateKey endState = {-1, -1};

    while (!pq.empty()) {
        StateKey u = pq.top().second;
        double current_time = pq.top().first;
        pq.pop();

        if (u.first == end) {
            // Dijkstra pops states in strictly increasing time order, so the
            // first (end, *any line*) state popped is the global minimum
            // across every possible arrival line - no need to keep
            // searching once this happens.
            found = true;
            endState = u;
            break;
        }

        if (current_time > time[u]) continue; // stale queue entry

        for (const Edge& edge : network.getNeighbors(u.first)) {
            int v = edge.to;

            if (delayService.isTrackClosed(u.first, v)) {
                continue;
            }

            double activeDelay = delayService.getDelay(u.first, v);
            bool isTransfer = (u.second != -1 && u.second != edge.lineId);
            double weight = edge.time + activeDelay + (isTransfer ? INTERCHANGE_PENALTY_MINUTES : 0.0);

            StateKey nextState = {v, edge.lineId};
            if (time.find(nextState) == time.end()) {
                time[nextState] = std::numeric_limits<double>::max();
            }

            if (current_time + weight < time[nextState]) {
                time[nextState] = current_time + weight;
                parent[nextState] = u;
                pq.push({time[nextState], nextState});
            }
        }
    }

    RouteResult result;

    if (!found) {
        result.totalMetric = 0.0;
        return result;
    }

    result.totalMetric = time[endState];

    // Reconstruct the path, dropping the line component of each state -
    // the caller only wants the sequence of stations.
    for (StateKey at = endState; ; at = parent[at]) {
        result.path.push_back(at.first);
        if (at == startState) break;
    }

    std::reverse(result.path.begin(), result.path.end());

    return result;
}