#include "../../include/network/MetroNetwork.h"
#include <iostream>
#include <mutex>
#include <shared_mutex>

// Private Constructor
MetroNetwork::MetroNetwork() {
    // Initialization logic can go here if needed in the future
}

// Singleton Instance Management (Meyers Singleton)
MetroNetwork& MetroNetwork::getInstance() {
    // In C++11 and later, static local variable initialization is thread-safe
    static MetroNetwork instance;
    return instance;
}

// ==========================================
// WRITE OPERATIONS (Require unique_lock)
// ==========================================

void MetroNetwork::addStation(int id, std::string name, bool isInterchange) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);

    stationToId[name] = id;
    idToStation[id] = Station(id, name, isInterchange);
}

void MetroNetwork::addRoute(int from, int to, double time, double dist, int lineId) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);

    adjList[from].push_back(Edge(to, time, dist, lineId));
    adjList[to].push_back(Edge(from, time, dist, lineId));
}

void MetroNetwork::addLine(int id, std::string name) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);
    lineNameToId[name] = id;
    lineIdToName[id] = name;
}

// ==========================================
// READ OPERATIONS (Require shared_lock)
// ==========================================

std::vector<Edge> MetroNetwork::getNeighbors(int stationId) const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);

    auto it = adjList.find(stationId);
    if (it != adjList.end()) {
        return it->second;
    }
    return {};
}

int MetroNetwork::getStationId(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);

    auto it = stationToId.find(name);
    if (it != stationToId.end()) {
        return it->second;
    }
    return -1;
}

std::string MetroNetwork::getStationName(int id) const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);

    auto it = idToStation.find(id);
    if (it != idToStation.end()) {
        return it->second.name;
    }
    return "Unknown";
}

int MetroNetwork::getTotalStations() const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    return stationToId.size();
}

int MetroNetwork::getLineId(const std::string& name) const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    auto it = lineNameToId.find(name);
    if (it != lineNameToId.end()) {
        return it->second;
    }
    return -1;
}

std::string MetroNetwork::getLineName(int id) const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    auto it = lineIdToName.find(id);
    if (it != lineIdToName.end()) {
        return it->second;
    }
    return "Unknown";
}