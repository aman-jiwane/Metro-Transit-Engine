#pragma once
#include <string>
#include <chrono>

enum class IncidentType {
    MAINTENANCE, SIGNAL_FAILURE, TRAIN_BREAKDOWN, HEAVY_LOAD, EMERGENCY, WEATHER, OTHER
};

enum class Severity {
    MINOR, MODERATE, MAJOR, CRITICAL, TRACK_CLOSED
};

struct DelayEvent {
    int eventId;
    int fromStation;
    int toStation;
    int lineId; // Which line this incident is on - lets DelayService notify
                // only the subscribers registered for that specific line.
    IncidentType type;
    Severity severity;
    double delayMinutes;
    bool trackClosed;
    bool active;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::string reportedBy;
};