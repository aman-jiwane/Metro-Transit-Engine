-- ============================================================================
-- Migration 008: Seed fare slabs
-- ----------------------------------------------------------------------------
-- These six rows are exactly the tiers that were previously hardcoded as an
-- if/else chain in FareCalculator.cpp (<=2km:Rs10, <=5km:Rs15, <=9km:Rs20,
-- <=14km:Rs25, <=20km:Rs30, above:Rs35). 999.0 stands in for "no upper
-- limit" - large enough that no possible route on this network can exceed
-- it, so the last slab always matches.
-- ============================================================================

PRAGMA foreign_keys = ON;

INSERT INTO fare_slabs (slab_id, min_distance_km, max_distance_km, fare) VALUES (1, 0.0,  2.0,   10.0);
INSERT INTO fare_slabs (slab_id, min_distance_km, max_distance_km, fare) VALUES (2, 2.0,  5.0,   15.0);
INSERT INTO fare_slabs (slab_id, min_distance_km, max_distance_km, fare) VALUES (3, 5.0,  9.0,   20.0);
INSERT INTO fare_slabs (slab_id, min_distance_km, max_distance_km, fare) VALUES (4, 9.0,  14.0,  25.0);
INSERT INTO fare_slabs (slab_id, min_distance_km, max_distance_km, fare) VALUES (5, 14.0, 20.0,  30.0);
INSERT INTO fare_slabs (slab_id, min_distance_km, max_distance_km, fare) VALUES (6, 20.0, 999.0, 35.0);