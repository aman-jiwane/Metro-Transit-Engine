#pragma once
#include <string>
#include <vector>

struct StationRecord {
    int id;
    std::string name;
    bool isInterchange;
};

struct RouteRecord {
    int lineId;
    int stationA;
    int stationB;
    double timeMinutes;
    double distanceKm;
};

struct LineRecord {
    int id;
    std::string name;
};

// Read-only access to the static network topology (lines, stations, routes).
// This is the "cache-aside" side of the architecture: MetroNetwork's
// in-memory graph is loaded from these once at startup and never re-queries
// the DB afterward, since the physical network doesn't change while the
// app is running.
class INetworkRepository {
public:
    virtual ~INetworkRepository() = default;
    virtual std::vector<LineRecord> getAllLines() = 0;
    virtual std::vector<StationRecord> getAllStations() = 0;
    virtual std::vector<RouteRecord> getAllRoutes() = 0;
};