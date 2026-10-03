#pragma once
#include "INetworkRepository.h"

class Database;

class SqliteNetworkRepository : public INetworkRepository {
private:
    Database& db;

public:
    explicit SqliteNetworkRepository(Database& database);

    std::vector<LineRecord> getAllLines() override;
    std::vector<StationRecord> getAllStations() override;
    std::vector<RouteRecord> getAllRoutes() override;
};