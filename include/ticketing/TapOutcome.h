#pragma once
#include <string>

// Result of a single tapCard() call. Carries everything a caller (CLI text,
// JSON API response) needs to present the outcome, instead of Turnstile
// printing to cout directly - that was fine when there was only one
// interface, but a REST API needs the same information as structured data.
struct TapOutcome {
    bool success = false;
    std::string message; // human-readable summary or reason for denial

    // Only meaningful when this was a checkout (isCheckout == true) and
    // success == true.
    bool isCheckout = false;
    std::string entryStationName;
    std::string exitStationName;
    std::string fareType;
    double fareCharged = 0.0;
    double remainingBalance = 0.0;
};