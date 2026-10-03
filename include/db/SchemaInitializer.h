#pragma once

class Database;

// On first run against a fresh database file, applies every migration in
// db/migrations/ (001 through 009, in order - schema, trigger, seed data).
// On subsequent runs it detects the schema already exists and does nothing,
// so starting the app twice against the same .db file is safe.
class SchemaInitializer {
public:
    static void ensureSchema(Database& db);
};