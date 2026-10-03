-- ============================================================================
-- Migration 005: Audit trigger
-- ----------------------------------------------------------------------------
-- Builds on 002_ticketing.sql (smart_cards) and 004_audit.sql (audit_log).
--
-- This is the "real DBMS talking point" piece: the audit trail is populated
-- by the database itself whenever smart_cards.balance changes, not by
-- application code remembering to log it. Even a bug in the C++ layer that
-- forgets to write an audit row cannot produce an un-audited balance change,
-- because the write to audit_log happens as a direct consequence of the
-- write to balance, inside the same statement.
--
-- The trigger deliberately does NOT try to set trip_id - it only sees
-- OLD/NEW column values on smart_cards, it has no way to know which trip (if
-- any) caused the change. trip_id is left NULL here and backfilled by the
-- application immediately afterward using sqlite3_last_insert_rowid(), which
-- correctly returns this trigger-inserted row's id right after the UPDATE
-- statement that fired it (SQLite guarantees this - triggers execute
-- synchronously as part of the statement that fires them). Recharges have no
-- trip to link, so their audit rows simply keep trip_id = NULL permanently.
--
-- NOTE ON SCOPE: this only fires on UPDATE. A new card's initial balance is
-- set via INSERT, not UPDATE, so account creation itself is not audit
-- logged here - audit_log tracks changes to an existing balance, not account
-- opening. An `AFTER INSERT ON smart_cards` trigger would be the clean
-- extension if account-opening events need to be logged too.
-- ============================================================================

PRAGMA foreign_keys = ON;

CREATE TRIGGER trg_audit_balance_change
AFTER UPDATE OF balance ON smart_cards
FOR EACH ROW
WHEN NEW.balance != OLD.balance
BEGIN
    INSERT INTO audit_log (card_id, change_type, amount, balance_after)
    VALUES (
        NEW.card_id,
        CASE WHEN NEW.balance > OLD.balance THEN 'RECHARGE' ELSE 'FARE_DEDUCTION' END,
        ABS(NEW.balance - OLD.balance),
        NEW.balance
    );
END;