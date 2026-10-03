#pragma once
#include <string>
#include <mutex>

// Forward-declared so headers that only need a Database& don't have to pull
// in the SQLite C API. Only Database.cpp includes <sqlite3.h>.
struct sqlite3;

// RAII wrapper around a single SQLite connection. Opens PRAGMA foreign_keys
// ON immediately, since SQLite treats that as a per-connection setting that
// is NOT persisted in the database file - every connection has to set it
// itself or foreign key constraints are silently unenforced.
//
// THREADING NOTE: SQLite's default "serialized" threading mode makes each
// individual API call (a single exec, a single step) safe to call from any
// thread. It does NOT make a multi-statement transaction
// (BEGIN ... several statements ... COMMIT) safe if two threads interleave
// their own BEGIN/COMMIT pairs on this SAME connection - a connection can
// only have one transaction open at a time, and two overlapping ones would
// corrupt each other's logical boundaries. transactionMutex exists to
// serialize exactly that: any repository method that writes to the
// database - whether a bare single UPDATE or a full multi-statement
// transaction - acquires it via acquireTransactionLock() and holds it for
// the entire operation.
class Database {
private:
    sqlite3* db = nullptr;
    std::mutex transactionMutex;

public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    sqlite3* handle() const { return db; }

    // Runs one or more semicolon-separated statements with no bound
    // parameters (used for schema/seed scripts and BEGIN/COMMIT/ROLLBACK).
    // Throws std::runtime_error on failure.
    void exec(const std::string& sql);

    long long lastInsertRowId() const;

    void begin();
    void commit();
    void rollback();

    // Held by a repository method for the duration of any write against
    // this connection - see the threading note above.
    std::unique_lock<std::mutex> acquireTransactionLock();
};