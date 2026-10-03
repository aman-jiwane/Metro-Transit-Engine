#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../../include/db/Database.h"
#include "../../include/db/SchemaInitializer.h"
#include "../../include/db/Statement.h"
#include "../../include/ticketing/SqliteCardRepository.h"
#include "../../include/ticketing/SqliteTripRepository.h"
#include "../../include/ticketing/SqliteFareRepository.h"
#include "../../include/network/SqliteNetworkRepository.h"
#include <cstdio>
#include <memory>

using Catch::Matchers::WithinAbs;

namespace {
// Creates a fresh temp SQLite file, applies every real migration to it
// (the exact same files main.cpp uses), and deletes it afterward. Assumes
// tests are run from the project root, same as the app itself, since
// SchemaInitializer reads db/migrations/ relative to the working directory.
struct TempDatabase {
    std::string path;
    std::unique_ptr<Database> db;

    TempDatabase() : path("test_integration_tmp.db") {
        std::remove(path.c_str());
        db = std::make_unique<Database>(path);
        SchemaInitializer::ensureSchema(*db);
    }

    ~TempDatabase() {
        db.reset();
        std::remove(path.c_str());
    }

    Database& operator*() { return *db; }
    Database* operator->() { return db.get(); }
};
}

TEST_CASE("Migrations seed the real Pune network into a fresh database", "[integration]") {
    TempDatabase temp;
    SqliteNetworkRepository repo(*temp);

    REQUIRE(repo.getAllStations().size() == 59);
    REQUIRE(repo.getAllRoutes().size() == 59);
    REQUIRE(repo.getAllLines().size() == 4);
}

TEST_CASE("Fare slabs are seeded and match the original tiers", "[integration]") {
    TempDatabase temp;
    SqliteFareRepository repo(*temp);

    REQUIRE_THAT(repo.getFareForDistance(1.0), WithinAbs(10.0, 1e-9));
    REQUIRE_THAT(repo.getFareForDistance(12.0), WithinAbs(25.0, 1e-9));
    REQUIRE_THAT(repo.getFareForDistance(100.0), WithinAbs(35.0, 1e-9));
}

TEST_CASE("SchemaInitializer is idempotent - running it twice does not re-seed or error", "[integration]") {
    TempDatabase temp;
    SchemaInitializer::ensureSchema(*temp); // second call, should be a no-op

    SqliteNetworkRepository repo(*temp);
    REQUIRE(repo.getAllStations().size() == 59); // not 118
}

TEST_CASE("SqliteCardRepository create/find/recharge round-trip correctly", "[integration]") {
    TempDatabase temp;
    SqliteCardRepository repo(*temp);

    REQUIRE_FALSE(repo.exists("CARD1"));
    repo.create("CARD1", CardType::STUDENT, 50.0);
    REQUIRE(repo.exists("CARD1"));

    CardRecord rec;
    REQUIRE(repo.find("CARD1", rec));
    REQUIRE(rec.cardType == CardType::STUDENT);
    REQUIRE_THAT(rec.balance, WithinAbs(50.0, 1e-9));
    REQUIRE(rec.entryStationId == -1);

    repo.recharge("CARD1", 25.0);
    repo.find("CARD1", rec);
    REQUIRE_THAT(rec.balance, WithinAbs(75.0, 1e-9));
}

TEST_CASE("setEntryStation sets and clears entry_station_id/entry_time together", "[integration]") {
    TempDatabase temp;
    SqliteCardRepository repo(*temp);
    repo.create("CARD1", CardType::REGULAR, 50.0);

    repo.setEntryStation("CARD1", 10);
    CardRecord rec;
    repo.find("CARD1", rec);
    REQUIRE(rec.entryStationId == 10);
    REQUIRE_FALSE(rec.entryTime.empty());

    repo.setEntryStation("CARD1", -1);
    repo.find("CARD1", rec);
    REQUIRE(rec.entryStationId == -1);
    REQUIRE(rec.entryTime.empty());
}

TEST_CASE("recordTrip deducts balance, inserts a trip, and links the audit_log row via trip_id", "[integration]") {
    TempDatabase temp;
    SqliteCardRepository cardRepo(*temp);
    SqliteTripRepository tripRepo(*temp);

    cardRepo.create("CARD1", CardType::REGULAR, 100.0);
    cardRepo.setEntryStation("CARD1", 0);

    TripOutcome outcome = tripRepo.recordTrip("CARD1", 0, "2026-01-01T00:00:00Z", 5, 3.0, "Regular", 15.0);

    REQUIRE(outcome.success);
    REQUIRE_THAT(outcome.newBalance, WithinAbs(85.0, 1e-9));

    CardRecord rec;
    cardRepo.find("CARD1", rec);
    REQUIRE_THAT(rec.balance, WithinAbs(85.0, 1e-9));
    REQUIRE(rec.entryStationId == -1); // cleared by the transaction

    Statement tripCheck(*temp, "SELECT COUNT(*) FROM trips WHERE card_id = 'CARD1';");
    tripCheck.step();
    REQUIRE(tripCheck.columnInt(0) == 1);

    Statement auditCheck(*temp,
        "SELECT change_type, amount, balance_after, trip_id FROM audit_log WHERE card_id = 'CARD1';");
    REQUIRE(auditCheck.step());
    REQUIRE(auditCheck.columnText(0) == "FARE_DEDUCTION");
    REQUIRE_THAT(auditCheck.columnDouble(1), WithinAbs(15.0, 1e-9));
    REQUIRE_THAT(auditCheck.columnDouble(2), WithinAbs(85.0, 1e-9));
    REQUIRE_FALSE(auditCheck.columnIsNull(3)); // trip_id must be backfilled, not left NULL
}

TEST_CASE("recordTrip rolls back completely on insufficient balance - no partial writes", "[integration]") {
    TempDatabase temp;
    SqliteCardRepository cardRepo(*temp);
    SqliteTripRepository tripRepo(*temp);

    cardRepo.create("CARD1", CardType::REGULAR, 10.0);
    cardRepo.setEntryStation("CARD1", 0);

    TripOutcome outcome = tripRepo.recordTrip("CARD1", 0, "2026-01-01T00:00:00Z", 5, 3.0, "Regular", 15.0);

    REQUIRE_FALSE(outcome.success);

    CardRecord rec;
    cardRepo.find("CARD1", rec);
    REQUIRE_THAT(rec.balance, WithinAbs(10.0, 1e-9)); // untouched
    REQUIRE(rec.entryStationId == 0); // still checked in

    Statement tripCheck(*temp, "SELECT COUNT(*) FROM trips;");
    tripCheck.step();
    REQUIRE(tripCheck.columnInt(0) == 0);

    Statement auditCheck(*temp, "SELECT COUNT(*) FROM audit_log;");
    auditCheck.step();
    REQUIRE(auditCheck.columnInt(0) == 0); // the failed UPDATE was rolled back, so the trigger's insert was too
}

TEST_CASE("The audit trigger fires on a plain recharge with no trip attached", "[integration]") {
    TempDatabase temp;
    SqliteCardRepository cardRepo(*temp);
    cardRepo.create("CARD1", CardType::REGULAR, 50.0);

    cardRepo.recharge("CARD1", 20.0);

    Statement auditCheck(*temp,
        "SELECT change_type, amount, balance_after, trip_id FROM audit_log WHERE card_id = 'CARD1';");
    REQUIRE(auditCheck.step());
    REQUIRE(auditCheck.columnText(0) == "RECHARGE");
    REQUIRE_THAT(auditCheck.columnDouble(1), WithinAbs(20.0, 1e-9));
    REQUIRE_THAT(auditCheck.columnDouble(2), WithinAbs(70.0, 1e-9));
    REQUIRE(auditCheck.columnIsNull(3)); // no trip - trip_id correctly stays NULL
}

TEST_CASE("Database constraints reject invalid data even through direct SQL", "[integration]") {
    TempDatabase temp;

    REQUIRE_THROWS_AS(temp->exec("INSERT INTO smart_cards (card_id, card_type, balance) VALUES ('X', 'REGULAR', -5.0);"),
                       std::runtime_error);
    REQUIRE_THROWS_AS(temp->exec("INSERT INTO smart_cards (card_id, card_type, balance) VALUES ('Y', 'VIP', 5.0);"),
                       std::runtime_error);
}