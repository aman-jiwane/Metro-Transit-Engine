-- ============================================================================
-- Migration 004: Audit trail
-- ----------------------------------------------------------------------------
-- Builds on 002_ticketing.sql (references smart_cards, trips).
-- ============================================================================

PRAGMA foreign_keys = ON;

-- ----------------------------------------------------------------------------
-- audit_log: append-only record of every balance change.
--
-- AUTOINCREMENT is used here - and deliberately NOT on the other tables in
-- this project - specifically so that once a log_id is issued it is never
-- reused, even if rows are later deleted. That's a basic integrity property
-- an audit trail should hold that the other tables don't need.
--
-- trip_id is nullable because a balance change can happen with no trip
-- attached (e.g. a recharge). amount is always stored as a positive number;
-- change_type carries the direction, so there is no sign-convention
-- ambiguity when reading old rows.
--
-- The trigger that populates this table automatically on every balance
-- change is Phase 3 work - it is tied to the checkout transaction Phase 3
-- implements, so it belongs with that integration code rather than here.
-- ----------------------------------------------------------------------------
CREATE TABLE audit_log (
    log_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    card_id         TEXT NOT NULL REFERENCES smart_cards(card_id),
    change_type     TEXT NOT NULL CHECK (change_type IN ('RECHARGE', 'FARE_DEDUCTION')),
    amount          REAL NOT NULL CHECK (amount > 0),
    balance_after   REAL NOT NULL CHECK (balance_after >= 0),
    trip_id         INTEGER REFERENCES trips(trip_id),
    occurred_at     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ', 'now'))
);

CREATE INDEX idx_audit_log_card_id ON audit_log(card_id);