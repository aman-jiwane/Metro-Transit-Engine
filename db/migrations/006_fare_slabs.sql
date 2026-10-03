-- ============================================================================
-- Migration 006: Fare slabs
-- ----------------------------------------------------------------------------
-- Moves the Pune Metro distance-based fare tiers out of the if/else chain
-- that previously lived in FareCalculator.cpp and into data. FareCalculator
-- now queries this table (loaded once at startup, same cache-aside approach
-- as the network graph) instead of hardcoding the breakpoints, so pricing
-- can change without a recompile.
--
-- UNIQUE(min_distance_km) catches the most common seed-data mistake (two
-- slabs accidentally starting at the same distance). It does NOT guarantee
-- the slabs have no gaps or overlaps between min/max boundaries across
-- different rows - SQLite CHECK constraints are per-row only and can't
-- express that. Full non-overlap validation is the seed data's
-- responsibility (verified for the seed values in 008_seed_fares.sql), not
-- something the schema enforces.
-- ============================================================================

PRAGMA foreign_keys = ON;

CREATE TABLE fare_slabs (
    slab_id          INTEGER PRIMARY KEY,
    min_distance_km  REAL NOT NULL CHECK (min_distance_km >= 0),
    max_distance_km  REAL NOT NULL CHECK (max_distance_km > min_distance_km),
    fare             REAL NOT NULL CHECK (fare > 0),
    UNIQUE (min_distance_km)
);