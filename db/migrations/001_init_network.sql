-- ============================================================================
-- Migration 001: Core network topology
-- ----------------------------------------------------------------------------
-- Establishes the static physical network: stations, lines, which stations
-- sit on which lines (many-to-many, since interchanges sit on 2+ lines), and
-- the physical track segments (routes) connecting adjacent stations.
--
-- NOTE ON PRAGMA foreign_keys: SQLite does not persist this setting inside
-- the database file - it is a per-connection setting. It is included here
-- for anyone running these files directly via the sqlite3 CLI, but the
-- Phase 3 C++ integration must also issue PRAGMA foreign_keys = ON on every
-- connection it opens, or foreign key constraints will be silently ignored.
-- ============================================================================

PRAGMA foreign_keys = ON;

-- ----------------------------------------------------------------------------
-- lines: the metro lines themselves (Purple, Aqua, Orange, Pink, ...)
-- ----------------------------------------------------------------------------
CREATE TABLE lines (
    line_id     INTEGER PRIMARY KEY,
    name        TEXT NOT NULL UNIQUE,
    color       TEXT
);

-- ----------------------------------------------------------------------------
-- stations
--
-- NORMALIZATION NOTE: is_interchange is technically derivable from
-- station_line (a station with rows on 2+ distinct line_ids is an
-- interchange), so storing it here is a deliberate denormalization, not an
-- oversight. Kept because:
--   1. Interchange status changes only when the physical network itself
--      changes - effectively never at runtime - so staleness risk is low.
--   2. It mirrors Station::isInterchange in the existing C++ domain model
--      exactly, so Phase 3 can hydrate a Station object straight from a
--      row with no derived-value logic in the loader.
-- ----------------------------------------------------------------------------
CREATE TABLE stations (
    station_id      INTEGER PRIMARY KEY,
    name            TEXT NOT NULL UNIQUE,
    is_interchange  INTEGER NOT NULL DEFAULT 0 CHECK (is_interchange IN (0, 1))
);

-- ----------------------------------------------------------------------------
-- station_line: many-to-many junction between stations and lines.
-- sequence_number orders stations along a line. It depends on the full
-- (station_id, line_id) composite key rather than just one half of it, so
-- this table satisfies 2NF.
-- ----------------------------------------------------------------------------
CREATE TABLE station_line (
    station_id      INTEGER NOT NULL REFERENCES stations(station_id),
    line_id         INTEGER NOT NULL REFERENCES lines(line_id),
    sequence_number INTEGER NOT NULL,
    PRIMARY KEY (station_id, line_id)
);

-- ----------------------------------------------------------------------------
-- routes: physical track segments.
--
-- Modeled as ONE row per undirected physical connection - NOT one row per
-- direction - specifically to avoid the update anomaly of two directional
-- rows for the same physical track drifting out of sync (e.g. someone
-- updates the time on one row but not its mirror). station_a_id < station_b_id
-- is enforced so the same physical track can never be stored twice in
-- swapped order. Phase 3's network loader is responsible for inserting each
-- route into the in-memory adjacency list in both directions, exactly as
-- MetroNetwork::addRoute already does today.
-- ----------------------------------------------------------------------------
CREATE TABLE routes (
    route_id        INTEGER PRIMARY KEY,
    line_id         INTEGER NOT NULL REFERENCES lines(line_id),
    station_a_id    INTEGER NOT NULL REFERENCES stations(station_id),
    station_b_id    INTEGER NOT NULL REFERENCES stations(station_id),
    time_minutes    REAL NOT NULL CHECK (time_minutes > 0),
    distance_km     REAL NOT NULL CHECK (distance_km > 0),
    CHECK (station_a_id < station_b_id),
    UNIQUE (station_a_id, station_b_id)
);

CREATE INDEX idx_routes_station_a ON routes(station_a_id);
CREATE INDEX idx_routes_station_b ON routes(station_b_id);