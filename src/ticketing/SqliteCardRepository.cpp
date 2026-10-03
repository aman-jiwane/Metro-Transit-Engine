#include "../../include/ticketing/SqliteCardRepository.h"
#include "../../include/db/Database.h"
#include "../../include/db/Statement.h"
#include "../../include/db/TimeUtil.h"

namespace {

std::string cardTypeToDb(CardType t) {
    switch (t) {
        case CardType::STUDENT:        return "STUDENT";
        case CardType::SENIOR_CITIZEN: return "SENIOR_CITIZEN";
        case CardType::DAILY_PASS:     return "DAILY_PASS";
        case CardType::REGULAR:
        default:                       return "REGULAR";
    }
}

CardType cardTypeFromDb(const std::string& s) {
    if (s == "STUDENT") return CardType::STUDENT;
    if (s == "SENIOR_CITIZEN") return CardType::SENIOR_CITIZEN;
    if (s == "DAILY_PASS") return CardType::DAILY_PASS;
    return CardType::REGULAR;
}

} // namespace

SqliteCardRepository::SqliteCardRepository(Database& database) : db(database) {}

bool SqliteCardRepository::exists(const std::string& cardId) {
    Statement stmt(db, "SELECT 1 FROM smart_cards WHERE card_id = ?;");
    stmt.bindText(1, cardId);
    return stmt.step();
}

void SqliteCardRepository::create(const std::string& cardId, CardType type, double initialBalance) {
    auto lock = db.acquireTransactionLock();
    Statement stmt(db, "INSERT INTO smart_cards (card_id, card_type, balance) VALUES (?, ?, ?);");
    stmt.bindText(1, cardId);
    stmt.bindText(2, cardTypeToDb(type));
    stmt.bindDouble(3, initialBalance);
    stmt.step();
}

bool SqliteCardRepository::find(const std::string& cardId, CardRecord& out) {
    Statement stmt(db, "SELECT card_id, card_type, balance, entry_station_id, entry_time FROM smart_cards WHERE card_id = ?;");
    stmt.bindText(1, cardId);
    if (!stmt.step()) return false;

    out.cardId = stmt.columnText(0);
    out.cardType = cardTypeFromDb(stmt.columnText(1));
    out.balance = stmt.columnDouble(2);
    out.entryStationId = stmt.columnIsNull(3) ? -1 : stmt.columnInt(3);
    out.entryTime = stmt.columnIsNull(4) ? "" : stmt.columnText(4);
    return true;
}

void SqliteCardRepository::recharge(const std::string& cardId, double amount) {
    auto lock = db.acquireTransactionLock();
    Statement stmt(db, "UPDATE smart_cards SET balance = balance + ? WHERE card_id = ?;");
    stmt.bindDouble(1, amount);
    stmt.bindText(2, cardId);
    stmt.step();
}

void SqliteCardRepository::setEntryStation(const std::string& cardId, int stationId) {
    auto lock = db.acquireTransactionLock();
    Statement stmt(db, "UPDATE smart_cards SET entry_station_id = ?, entry_time = ? WHERE card_id = ?;");
    if (stationId == -1) {
        stmt.bindNull(1);
        stmt.bindNull(2);
    } else {
        stmt.bindInt(1, stationId);
        stmt.bindText(2, nowIso8601());
    }
    stmt.bindText(3, cardId);
    stmt.step();
}