#include "../../include/db/Statement.h"
#include "../../include/db/Database.h"
#include <sqlite3.h>
#include <stdexcept>

Statement::Statement(const Database& db, const std::string& sql) {
    int rc = sqlite3_prepare_v2(db.handle(), sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " +
            std::string(sqlite3_errmsg(db.handle())) + " (" + sql + ")");
    }
}

Statement::~Statement() {
    if (stmt) sqlite3_finalize(stmt);
}

void Statement::bindInt(int index, int value) {
    sqlite3_bind_int(stmt, index, value);
}

void Statement::bindInt64(int index, long long value) {
    sqlite3_bind_int64(stmt, index, value);
}

void Statement::bindDouble(int index, double value) {
    sqlite3_bind_double(stmt, index, value);
}

void Statement::bindText(int index, const std::string& value) {
    sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void Statement::bindNull(int index) {
    sqlite3_bind_null(stmt, index);
}

bool Statement::step() {
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw std::runtime_error("Failed to execute statement: " +
        std::string(sqlite3_errmsg(sqlite3_db_handle(stmt))));
}

int Statement::columnInt(int index) const {
    return sqlite3_column_int(stmt, index);
}

long long Statement::columnInt64(int index) const {
    return sqlite3_column_int64(stmt, index);
}

double Statement::columnDouble(int index) const {
    return sqlite3_column_double(stmt, index);
}

std::string Statement::columnText(int index) const {
    const unsigned char* text = sqlite3_column_text(stmt, index);
    return text ? reinterpret_cast<const char*>(text) : "";
}

bool Statement::columnIsNull(int index) const {
    return sqlite3_column_type(stmt, index) == SQLITE_NULL;
}

void Statement::reset() {
    sqlite3_reset(stmt);
}