#pragma once

// Commuter category. Drives which FarePolicy the FareCalculatorFactory
// produces, and is stored directly on the smart_cards row in SQLite - the
// database is the source of truth for this now, not an in-memory object.
enum class CardType { REGULAR, STUDENT, SENIOR_CITIZEN, DAILY_PASS };