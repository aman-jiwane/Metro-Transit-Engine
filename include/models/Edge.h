#pragma once
struct Edge {
    int to;
    double time;
    double dist;
    int lineId; // Which metro line this physical track belongs to. Lets routing
                // algorithms detect a line change (interchange) along a path.

    Edge(int to, double time, double dist, int lineId)
        : to(to), time(time), dist(dist), lineId(lineId) {}
};

