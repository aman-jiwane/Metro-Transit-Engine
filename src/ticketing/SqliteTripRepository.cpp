#include "../../include/ticketing/SqliteTripRepository.h"
#include "../../include/db/Database.h"
#include "../../include/db/Statement.h"
#include "../../include/db/TimeUtil.h"

SqliteTripRepository::SqliteTripRepository(Database& database) : db(database) {}

TripOutcome SqliteTripRepository::recordTrip(
    const std::string& cardId,
    int entryStationId,
    const std::string& entryTime,
    int exitStationId,
    double distanceKm,
    const std::string& fareTypeUsed,
    double fareCharged
) {

    auto lock = db.acquireTransactionLock(); // held for the ENTIRE transaction below

    db.begin();

    // Re-read balance from inside the transaction, so the sufficiency check
    // reflects the authoritative current state rather than a value the
    // caller happened to read a moment earlier.
    double currentBalance;
    {
        Statement check(db, "SELECT balance FROM smart_cards WHERE card_id = ?;");
        check.bindText(1, cardId);
        if (!check.step()) {
            db.rollback();
            return { false, "Card not found.", 0.0 };
        }
        currentBalance = check.columnDouble(0);
    }

    if (currentBalance < fareCharged) {
        db.rollback();
        return { false, "Insufficient balance.", currentBalance };
    }

    // 1. Deduct the fare and clear check-in state. The audit trigger fires
    //    as part of this UPDATE and inserts an audit_log row with
    //    trip_id still NULL - it has no way to know the trip yet.
    {
        Statement deduct(db,
            "UPDATE smart_cards SET balance = balance - ?, entry_station_id = NULL, entry_time = NULL WHERE card_id = ?;");
        deduct.bindDouble(1, fareCharged);
        deduct.bindText(2, cardId);
        deduct.step();
    }
    long long auditLogId = db.lastInsertRowId();

    // 2. Insert the trip row.
    std::string exitTime = nowIso8601();
    {
        Statement insertTrip(db,
            "INSERT INTO trips (card_id, entry_station_id, exit_station_id, entry_time, exit_time, distance_km, fare_type_used, fare_charged) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?);");
        insertTrip.bindText(1, cardId);
        insertTrip.bindInt(2, entryStationId);
        insertTrip.bindInt(3, exitStationId);
        insertTrip.bindText(4, entryTime);
        insertTrip.bindText(5, exitTime);
        insertTrip.bindDouble(6, distanceKm);
        insertTrip.bindText(7, fareTypeUsed);
        insertTrip.bindDouble(8, fareCharged);
        insertTrip.step();
    }
    long long tripId = db.lastInsertRowId();

    // 3. Backfill trip_id onto the audit_log row the trigger created in step 1.
    {
        Statement link(db, "UPDATE audit_log SET trip_id = ? WHERE log_id = ?;");
        link.bindInt64(1, tripId);
        link.bindInt64(2, auditLogId);
        link.step();
    }

    db.commit();
    return { true, "", currentBalance - fareCharged };
}