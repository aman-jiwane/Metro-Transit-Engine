#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <shared_mutex>
#include "../models/Station.h"
#include "../models/Edge.h"

class MetroNetwork {
private:

    std::unordered_map<int, std::vector<Edge>> adjList;
    std::unordered_map<std::string, int> stationToId;
    std::unordered_map<int, Station> idToStation;
    std::unordered_map<std::string, int> lineNameToId;
    std::unordered_map<int, std::string> lineIdToName;

    mutable std::shared_mutex rw_lock;

public:
    // Public (not the strict private-constructor Singleton from earlier
    // phases) specifically so unit tests can construct small, isolated
    // instances instead of sharing global state through getInstance() and
    // needing to reset it between tests. Production code (main.cpp) still
    // goes through getInstance() for the one shared network the whole app
    // uses - this is a deliberate relaxation for testability, not a
    // reversal of the Singleton pattern itself.
    MetroNetwork();

    MetroNetwork(const MetroNetwork&) = delete;
    MetroNetwork& operator=(const MetroNetwork&) = delete;


    static MetroNetwork& getInstance();

    void addStation(int id, std::string name, bool isInterchange = false);
    void addRoute(int from, int to, double time, double dist, int lineId);
    void addLine(int id, std::string name);


    std::vector<Edge> getNeighbors(int stationId) const;
    int getStationId(const std::string& name) const;
    std::string getStationName(int id) const;
    int getLineId(const std::string& name) const;
    std::string getLineName(int id) const;
    int getTotalStations() const;
};