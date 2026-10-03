#include "../../include/network/SqliteNetworkRepository.h"
#include "../../include/db/Database.h"
#include "../../include/db/Statement.h"

SqliteNetworkRepository::SqliteNetworkRepository(Database& database) : db(database) {}

std::vector<LineRecord> SqliteNetworkRepository::getAllLines() {
    std::vector<LineRecord> result;
    Statement stmt(db, "SELECT line_id, name FROM lines;");
    while (stmt.step()) {
        LineRecord rec;
        rec.id = stmt.columnInt(0);
        rec.name = stmt.columnText(1);
        result.push_back(rec);
    }
    return result;
}

std::vector<StationRecord> SqliteNetworkRepository::getAllStations() {
    std::vector<StationRecord> result;
    Statement stmt(db, "SELECT station_id, name, is_interchange FROM stations;");
    while (stmt.step()) {
        StationRecord rec;
        rec.id = stmt.columnInt(0);
        rec.name = stmt.columnText(1);
        rec.isInterchange = stmt.columnInt(2) != 0;
        result.push_back(rec);
    }
    return result;
}

std::vector<RouteRecord> SqliteNetworkRepository::getAllRoutes() {
    std::vector<RouteRecord> result;
    Statement stmt(db, "SELECT line_id, station_a_id, station_b_id, time_minutes, distance_km FROM routes;");
    while (stmt.step()) {
        RouteRecord rec;
        rec.lineId = stmt.columnInt(0);
        rec.stationA = stmt.columnInt(1);
        rec.stationB = stmt.columnInt(2);
        rec.timeMinutes = stmt.columnDouble(3);
        rec.distanceKm = stmt.columnDouble(4);
        result.push_back(rec);
    }
    return result;
}