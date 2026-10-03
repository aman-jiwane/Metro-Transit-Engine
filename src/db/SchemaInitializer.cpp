#include "../../include/db/SchemaInitializer.h"
#include "../../include/db/Database.h"
#include "../../include/db/Statement.h"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <string>

namespace {

bool schemaAlreadyExists(Database& db) {
    Statement stmt(db, "SELECT name FROM sqlite_master WHERE type='table' AND name='stations';");
    return stmt.step(); // true if a row came back, i.e. the table exists
}

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error(
            "Could not open migration file '" + path + "'. "
            "Make sure the app is run from the project root (the same "
            "directory that contains the db/ folder), and that db/migrations/ "
            "was copied into the project."
        );
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace

void SchemaInitializer::ensureSchema(Database& db) {
    if (schemaAlreadyExists(db)) {
        return; // Already initialized on a previous run - nothing to do.
    }

    const std::vector<std::string> migrations = {
        "db/migrations/001_init_network.sql",
        "db/migrations/002_ticketing.sql",
        "db/migrations/003_operations.sql",
        "db/migrations/004_audit.sql",
        "db/migrations/005_audit_trigger.sql",
        "db/migrations/006_fare_slabs.sql",
        "db/migrations/007_seed_network.sql",
        "db/migrations/008_seed_fares.sql",
        "db/migrations/009_add_entry_time.sql",
    };

    for (const auto& path : migrations) {
        db.exec(readFile(path));
    }
}