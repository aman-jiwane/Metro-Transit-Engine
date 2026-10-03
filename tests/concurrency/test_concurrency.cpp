#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../../include/network/DelayService.h"
#include "../../include/db/Database.h"
#include "../../include/db/SchemaInitializer.h"
#include "../../include/db/Statement.h"
#include "../../include/ticketing/SqliteCardRepository.h"
#include "../../include/ticketing/SqliteTripRepository.h"
#include <thread>
#include <vector>
#include <atomic>
#include <cstdio>
#include <memory>

using Catch::Matchers::WithinAbs;

// ---------------------------------------------------------------------------
// DelayService: proves the shared_mutex protecting activeIncidents/subscriber
// lists holds up under real concurrent read+write pressure, not just that it
// compiles. Run under -fsanitize=address,undefined (or ThreadSanitizer) to
// actually catch a data race if one exists - a plain pass/fail run alone
// can miss races that don't happen to manifest on a given run.
// ---------------------------------------------------------------------------
TEST_CASE("DelayService survives concurrent reporting and querying from many threads", "[concurrency]") {
    DelayService svc;
    const int numReporterThreads = 10;
    const int incidentsPerThread = 50;
    std::atomic<int> totalReported{0};

    std::vector<std::thread> threads;

    // Reporter threads: each writes incidents on its own distinct station
    // pair range, so we can verify none are lost or duplicated.
    for (int t = 0; t < numReporterThreads; ++t) {
        threads.emplace_back([&svc, &totalReported, t, incidentsPerThread]() {
            for (int i = 0; i < incidentsPerThread; ++i) {
                svc.reportIncident(t * 1000 + i, t * 1000 + i + 1, 1,
                                    IncidentType::MAINTENANCE, Severity::MINOR, 60, "stress");
                totalReported++;
            }
        });
    }

    // Concurrent reader threads: hammer the read paths while writers are
    // still running, to actually exercise shared_lock vs unique_lock
    // contention. Bounded iteration count rather than "loop until writers
    // finish" - an unbounded busy-wait here runs extremely slowly under
    // sanitizer instrumentation (every memory access is instrumented),
    // which doesn't indicate a bug, just makes the test needlessly slow.
    std::vector<std::thread> readerThreads;
    for (int r = 0; r < 4; ++r) {
        readerThreads.emplace_back([&svc]() {
            for (int i = 0; i < 200; ++i) {
                svc.getActiveIncidents();
                svc.isTrackClosed(0, 1);
                svc.getDelay(0, 1);
            }
        });
    }

    for (auto& th : threads) th.join();
    for (auto& th : readerThreads) th.join();

    REQUIRE(totalReported == numReporterThreads * incidentsPerThread);
    REQUIRE(svc.getActiveIncidents().size() == static_cast<size_t>(numReporterThreads * incidentsPerThread));
}

// ---------------------------------------------------------------------------
// SqliteTripRepository::recordTrip: proves acquireTransactionLock() actually
// makes concurrent checkouts on the SAME card, sharing ONE connection, safe.
// Without that lock, two threads' BEGIN...COMMIT sequences on the same
// sqlite3* connection could interleave and corrupt each other's transaction
// boundaries - this test is what would have caught that bug.
// ---------------------------------------------------------------------------
TEST_CASE("Concurrent recordTrip calls on the same card never lose an update or go negative", "[concurrency]") {
    std::string path = "test_concurrency_tmp.db";
    std::remove(path.c_str());
    {
        Database db(path);
        SchemaInitializer::ensureSchema(db);

        SqliteCardRepository cardRepo(db);
        SqliteTripRepository tripRepo(db);

        const double startingBalance = 1000.0;
        const double farePerTrip = 10.0;
        const int numThreads = 20;

        cardRepo.create("STRESS1", CardType::REGULAR, startingBalance);
        cardRepo.setEntryStation("STRESS1", 0);

        std::atomic<int> successCount{0};
        std::atomic<int> failureCount{0};
        std::vector<std::thread> threads;

        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([&]() {
                TripOutcome outcome = tripRepo.recordTrip(
                    "STRESS1", 0, "2026-01-01T00:00:00Z", 1, 1.0, "Regular", farePerTrip
                );
                if (outcome.success) successCount++;
                else failureCount++;
            });
        }
        for (auto& th : threads) th.join();

        REQUIRE(successCount + failureCount == numThreads);

        CardRecord rec;
        cardRepo.find("STRESS1", rec);

        double expectedBalance = startingBalance - (successCount.load() * farePerTrip);
        REQUIRE_THAT(rec.balance, WithinAbs(expectedBalance, 1e-6));
        REQUIRE(rec.balance >= 0.0); // never went negative even under concurrent access

        // Every successful trip must have actually been recorded - proves no
        // successful deduction ever "lost" its matching trip row.
        Statement tripCount(db, "SELECT COUNT(*) FROM trips WHERE card_id = 'STRESS1';");
        tripCount.step();
        REQUIRE(tripCount.columnInt(0) == successCount.load());
    }
    std::remove(path.c_str());
}

// ---------------------------------------------------------------------------
// Mixed recharge + spend pressure on the same card: proves the transaction
// lock covers recharge() too (a single-statement write), not just the
// multi-statement recordTrip transaction - without that, a recharge could
// interleave inside another thread's open transaction on the shared connection.
// ---------------------------------------------------------------------------
TEST_CASE("Concurrent recharges and trips on the same card settle to a mathematically correct balance", "[concurrency]") {
    std::string path = "test_concurrency_tmp2.db";
    std::remove(path.c_str());
    {
        Database db(path);
        SchemaInitializer::ensureSchema(db);

        SqliteCardRepository cardRepo(db);
        SqliteTripRepository tripRepo(db);

        const double startingBalance = 500.0;
        const double rechargeAmount = 20.0;
        const double fare = 10.0;
        const int numRechargeThreads = 15;
        const int numTripThreads = 15;

        cardRepo.create("STRESS2", CardType::REGULAR, startingBalance);

        std::atomic<int> tripSuccesses{0};
        std::vector<std::thread> threads;

        for (int i = 0; i < numRechargeThreads; ++i) {
            threads.emplace_back([&]() {
                cardRepo.recharge("STRESS2", rechargeAmount);
            });
        }
        for (int i = 0; i < numTripThreads; ++i) {
            threads.emplace_back([&]() {
                cardRepo.setEntryStation("STRESS2", 0);
                TripOutcome outcome = tripRepo.recordTrip(
                    "STRESS2", 0, "2026-01-01T00:00:00Z", 1, 1.0, "Regular", fare
                );
                if (outcome.success) tripSuccesses++;
            });
        }
        for (auto& th : threads) th.join();

        CardRecord rec;
        cardRepo.find("STRESS2", rec);

        double expected = startingBalance
                         + (numRechargeThreads * rechargeAmount)
                         - (tripSuccesses.load() * fare);
        REQUIRE_THAT(rec.balance, WithinAbs(expected, 1e-6));
        REQUIRE(rec.balance >= 0.0);
    }
    std::remove(path.c_str());
}