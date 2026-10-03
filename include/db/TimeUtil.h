#pragma once
#include <string>

// Returns the current UTC time as an ISO-8601 string (e.g.
// "2026-08-01T10:15:00Z"), matching the format used throughout the schema's
// TEXT timestamp columns and SQLite's own strftime('%Y-%m-%dT%H:%M:%fZ') defaults.
std::string nowIso8601();