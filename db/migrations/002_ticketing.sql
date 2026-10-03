-- ============================================================================
-- Migration 002: Ticketing domain
-- ----------------------------------------------------------------------------
-- Builds on 001_init_network.sql (references stations).
-- ============================================================================

PRAGMA foreign_keys = ON;

-- ----------------------------------------------------------------------------
-- smart_cards
-- card_id is the human-facing ID already used throughout the C++ code (e.g.
-- "USER1"), so it is used directly as the primary key rather than adding a
-- surrogate integer key nobody would actually query by.
-- ----------------------------------------------------------------------------
CREATE TABLE smart_cards (
    card_id             TEXT PRIMARY KEY,
    card_type           TEXT NOT NULL CHECK (card_type IN ('REGULAR', 'STUDENT', 'SENIOR_CITIZEN', 'DAILY_PASS')),
    balance             REAL NOT NULL DEFAULT 0 CHECK (balance >= 0),
    entry_station_id    INTEGER REFERENCES stations(station_id),
    created_at          TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

-- ----------------------------------------------------------------------------
-- trips: a completed check-in/check-out journey.
--
-- NORMALIZATION NOTE: fare_type_used and distance_km are deliberately
-- snapshotted here rather than re-derived by joining smart_cards/routes at
-- query time. A historical trip must keep reflecting the fare rule and
-- distance that actually applied on the day it happened, even if the card's
-- type changes later or the network topology is edited - re-deriving them
-- from current state would silently rewrite history. distance_km in
-- particular cannot be recomputed from stored columns alone anyway, since it
-- depends on which route the pathfinding algorithm chose at the time.
-- ----------------------------------------------------------------------------
CREATE TABLE trips (
    trip_id             INTEGER PRIMARY KEY,
    card_id             TEXT NOT NULL REFERENCES smart_cards(card_id),
    entry_station_id    INTEGER NOT NULL REFERENCES stations(station_id),
    exit_station_id     INTEGER NOT NULL REFERENCES stations(station_id),
    entry_time          TEXT NOT NULL,
    exit_time           TEXT NOT NULL,
    distance_km         REAL NOT NULL CHECK (distance_km >= 0),
    fare_type_used      TEXT NOT NULL CHECK (fare_type_used IN ('Regular', 'Student', 'Senior Citizen', 'Daily Pass')),
    fare_charged        REAL NOT NULL CHECK (fare_charged >= 0),
    CHECK (exit_time >= entry_time)
);

CREATE INDEX idx_trips_card_id ON trips(card_id);
CREATE INDEX idx_trips_entry_time ON trips(entry_time);