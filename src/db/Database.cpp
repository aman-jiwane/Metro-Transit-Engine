#include "../../include/db/Database.h"
#include <sqlite3.h>
#include <stdexcept>

Database::Database(const std::string& path) {
    int rc = sqlite3_open(path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::string msg = db ? sqlite3_errmsg(db) : "unknown error";
        if (db) sqlite3_close(db);
        db = nullptr;
        throw std::runtime_error("Failed to open database '" + path + "': " + msg);
    }
    exec("PRAGMA foreign_keys = ON;");
}

Database::~Database() {
    if (db) {
        sqlite3_close(db);
    }
}

void Database::exec(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("SQL error: " + msg);
    }
}

long long Database::lastInsertRowId() const {
    return sqlite3_last_insert_rowid(db);
}

void Database::begin() {
    exec("BEGIN TRANSACTION;");
}

void Database::commit() {
    exec("COMMIT;");
}

void Database::rollback() {
    exec("ROLLBACK;");
}

std::unique_lock<std::mutex> Database::acquireTransactionLock() {
    return std::unique_lock<std::mutex>(transactionMutex);
}