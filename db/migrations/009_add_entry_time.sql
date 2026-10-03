-- ============================================================================
-- Migration 009: Track entry time on smart_cards
-- ----------------------------------------------------------------------------
-- 002_ticketing.sql gave smart_cards an entry_station_id to track where a
-- card checked in, but nothing to track WHEN. Without it, trips.entry_time
-- can only ever be stamped at checkout, which is simply wrong. This adds a
-- nullable entry_time column with the same lifecycle as entry_station_id:
-- set together at check-in, cleared together at check-out.
-- ============================================================================

ALTER TABLE smart_cards ADD COLUMN entry_time TEXT;