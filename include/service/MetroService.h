#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "../network/MetroNetwork.h"
#include "../network/DelayService.h"
#include "../ticketing/ICardRepository.h"
#include "../ticketing/ITripRepository.h"
#include "../ticketing/IFareRepository.h"
#include "../ticketing/Turnstile.h"
#include "../ticketing/CardIncidentSubscriber.h"
#include "../routing/CachedShortestTimeStrategy.h"

// ---------------------------------------------------------------------------
// Result types. Deliberately plain data - no cout, no JSON, no formatting
// decisions. The CLI prints these as text; the REST API serializes them as
// JSON. Neither interface owns the business logic that produces them.
// ---------------------------------------------------------------------------

struct CardInfoResult {
    bool success = false;
    std::string message;
    bool created = false; // true if this call created a NEW card, false if it recharged an existing one
    std::string cardId;
    std::string cardType;
    double balance = 0.0;
    bool checkedIn = false;
    std::string checkedInStation;
};

struct RouteSearchResult {
    bool success = false;
    std::string message;
    std::vector<std::string> stationNames;
    double metric = 0.0;
    bool isTimeMetric = true; // true = minutes (Dijkstra), false = stop count (BFS)
};

struct IncidentActionResult {
    bool success = false;
    std::string message;
    int eventId = -1;
    std::string lineName;
};

struct IncidentSummary {
    int eventId;
    std::string fromStationName;
    std::string toStationName;
    std::string lineName;
    std::string incidentType;
    std::string severity;
    double delayMinutes;
    bool trackClosed;
    std::string reportedBy;
};

struct SimpleResult {
    bool success = false;
    std::string message;
};

// ---------------------------------------------------------------------------
// MetroService: every operation the system supports, as one clean method
// each. Constructed once and shared - main.cpp (CLI) and api_main.cpp (REST
// server) each build their own MetroService instance over the same
// metro_core components, but the class itself has zero knowledge of which
// interface is calling it.
// ---------------------------------------------------------------------------
class MetroService {
private:
    MetroNetwork& network;
    DelayService& delayService;
    ICardRepository& cardRepo;
    CachedShortestTimeStrategy& cachedRouter;
    Turnstile& turnstile;

    std::unordered_map<std::string, std::unique_ptr<CardIncidentSubscriber>> subscribers;

    // Returns the line connecting two adjacent stations, or -1 if no direct
    // track exists between them.
    int findDirectLine(int stationA, int stationB) const;

public:
    MetroService(MetroNetwork& net, DelayService& delaySvc, ICardRepository& cardRepository,
                 CachedShortestTimeStrategy& router, Turnstile& gate);

    // ---- Ticketing ----
    // cardTypeStr: "Regular" | "Student" | "SeniorCitizen" | "DailyPass" (case-insensitive).
    // Creates a new card if cardId doesn't exist yet, otherwise recharges it.
    CardInfoResult createOrRechargeCard(const std::string& cardId, const std::string& cardTypeStr, double amount);
    CardInfoResult getCardInfo(const std::string& cardId);

    // ---- Routing ----
    // strategy: "fastest" (Dijkstra, cached, interchange-penalty-aware) or "fewest" (BFS).
    RouteSearchResult searchRoute(const std::string& startStationName, const std::string& endStationName, const std::string& strategy);

    // ---- Turnstile ----
    TapOutcome tapIn(const std::string& cardId, const std::string& stationName);
    TapOutcome tapOut(const std::string& cardId, const std::string& stationName);
    SimpleResult setTurnstileOutOfOrder();
    SimpleResult repairTurnstile();
    std::string getTurnstileStatus() const;

    // ---- Admin: incidents ----
    // typeStr: "Maintenance" | "SignalFailure" | "TrainBreakdown" | "HeavyLoad" | "Emergency" | "Weather" | "Other".
    // severityStr: "Minor" | "Moderate" | "Major" | "Critical".
    IncidentActionResult reportIncident(const std::string& fromStationName, const std::string& toStationName,
                                         const std::string& typeStr, const std::string& severityStr,
                                         int durationMinutes, const std::string& reportedBy);
    SimpleResult removeIncident(int eventId);
    IncidentActionResult closeTrack(const std::string& fromStationName, const std::string& toStationName, int durationMinutes);
    SimpleResult reopenTrack(const std::string& fromStationName, const std::string& toStationName);
    std::vector<IncidentSummary> getActiveIncidents() const;

    // ---- Observer subscriptions ----
    SimpleResult subscribeToLine(const std::string& cardId, const std::string& lineName);
};