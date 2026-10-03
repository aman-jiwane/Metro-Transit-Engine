#pragma once
#include <string>

struct sqlite3_stmt;
class Database;

// RAII wrapper around a prepared statement. bind*/step/column* mirror the
// SQLite C API 1:1 but throw std::runtime_error instead of returning error
// codes, and the destructor finalizes the statement automatically so
// repository methods can't leak a prepared statement on an early return.
class Statement {
private:
    sqlite3_stmt* stmt = nullptr;

public:
    Statement(const Database& db, const std::string& sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    void bindInt(int index, int value);
    void bindInt64(int index, long long value);
    void bindDouble(int index, double value);
    void bindText(int index, const std::string& value);
    void bindNull(int index);

    // Advances one row. Returns true if a row is available (caller should
    // read columns), false if the statement has no more rows.
    bool step();

    int columnInt(int index) const;
    long long columnInt64(int index) const;
    double columnDouble(int index) const;
    std::string columnText(int index) const;
    bool columnIsNull(int index) const;

    void reset();
};