#include "../../include/routing/FewestStations.h"
#include "../../include/network/MetroNetwork.h"
#include "../../include/network/DelayService.h"
#include <queue>
#include <unordered_map>
#include <algorithm>

RouteResult FewestStationsStrategy::findPath(const MetroNetwork& network, const DelayService& delayService, int start, int end) {
    std::unordered_map<int, bool> visited;
    std::unordered_map<int, int> parent;
    std::queue<int> q;

    q.push(start);
    visited[start] = true;
    parent[start] = -1;

    bool found = false;

    while (!q.empty()) {
        int u = q.front();
        q.pop();

        if (u == end) {
            found = true;
            break;
        }

        for (const Edge& edge : network.getNeighbors(u)) {
            int v = edge.to;

            if (delayService.isTrackClosed(u, v)) {
                continue;
            }

            if (!visited[v]) {
                visited[v] = true;
                parent[v] = u;
                q.push(v);
            }
        }
    }

    RouteResult result;

    if (!found) {
        result.totalMetric = 0.0;
        return result;
    }

    for (int at = end; at != -1; at = parent[at]) {
        result.path.push_back(at);
    }
    std::reverse(result.path.begin(), result.path.end());

    result.totalMetric = result.path.size() - 1;
    return result;
}