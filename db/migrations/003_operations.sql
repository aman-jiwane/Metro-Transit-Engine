-- ============================================================================
-- Migration 003: Real-time operations domain
-- ----------------------------------------------------------------------------
-- Builds on 001_init_network.sql (references stations).
-- ============================================================================

PRAGMA foreign_keys = ON;

-- ----------------------------------------------------------------------------
-- delay_events
--
-- NORMALIZATION NOTE: under the current business rules, severity
-- functionally determines both delay_minutes and track_closed
-- (Minor=2min, Moderate=5min, Major=10min, Critical=20min, TrackClosed=0min
-- /closed). A strict 3NF design would extract that mapping into its own
-- severity_levels(severity_code, default_delay_minutes) reference table.
-- We deliberately keep delay_minutes and track_closed as direct columns
-- instead, because:
--   1. The mapping is a small, fixed set of 5 business rules already
--      validated in the C++ layer (DelayService::reportIncident) - a
--      reference table for 5 static rows buys no real flexibility today.
--   2. These two columns sit on the routing hot path - ShortestTime and
--      FewestStations call isTrackClosed()/getDelay() per edge, per
--      search. Avoiding a join on every edge check matters here.
--   3. The CHECK constraint below closes the main anomaly risk that
--      normalization exists to prevent (severity and track_closed
--      silently drifting apart), without paying for a join.
-- If per-incident override of the default delay ever becomes a real
-- requirement, the clean extension is a nullable delay_minutes_override
-- column, not a full reference table.
-- ----------------------------------------------------------------------------
CREATE TABLE delay_events (
    event_id        INTEGER PRIMARY KEY,
    from_station_id INTEGER NOT NULL REFERENCES stations(station_id),
    to_station_id   INTEGER NOT NULL REFERENCES stations(station_id),
    incident_type   TEXT NOT NULL CHECK (incident_type IN
                        ('MAINTENANCE', 'SIGNAL_FAILURE', 'TRAIN_BREAKDOWN',
                         'HEAVY_LOAD', 'EMERGENCY', 'WEATHER', 'OTHER')),
    severity        TEXT NOT NULL CHECK (severity IN
                        ('MINOR', 'MODERATE', 'MAJOR', 'CRITICAL', 'TRACK_CLOSED')),
    delay_minutes   REAL NOT NULL CHECK (delay_minutes >= 0),
    track_closed    INTEGER NOT NULL DEFAULT 0 CHECK (track_closed IN (0, 1)),
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0, 1)),
    start_time      TEXT NOT NULL,
    end_time        TEXT NOT NULL,
    reported_by     TEXT,
    CHECK (end_time >= start_time),
    CHECK ((severity = 'TRACK_CLOSED' AND track_closed = 1)
        OR (severity != 'TRACK_CLOSED' AND track_closed = 0))
);

CREATE INDEX idx_delay_events_active ON delay_events(active);
CREATE INDEX idx_delay_events_stations ON delay_events(from_station_id, to_station_id);