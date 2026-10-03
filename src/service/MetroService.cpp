#include "../../include/service/MetroService.h"
#include "../../include/routing/FewestStations.h"
#include <cctype>

namespace {

bool equalsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

CardType parseCardType(const std::string& s) {
    if (equalsIgnoreCase(s, "Student")) return CardType::STUDENT;
    if (equalsIgnoreCase(s, "SeniorCitizen")) return CardType::SENIOR_CITIZEN;
    if (equalsIgnoreCase(s, "DailyPass")) return CardType::DAILY_PASS;
    return CardType::REGULAR; // unrecognized input defaults to Regular
}

std::string cardTypeToString(CardType t) {
    switch (t) {
        case CardType::STUDENT:        return "Student";
        case CardType::SENIOR_CITIZEN: return "SeniorCitizen";
        case CardType::DAILY_PASS:     return "DailyPass";
        case CardType::REGULAR:
        default:                       return "Regular";
    }
}

bool parseIncidentType(const std::string& s, IncidentType& out) {
    if (equalsIgnoreCase(s, "Maintenance"))    { out = IncidentType::MAINTENANCE;     return true; }
    if (equalsIgnoreCase(s, "SignalFailure"))  { out = IncidentType::SIGNAL_FAILURE;  return true; }
    if (equalsIgnoreCase(s, "TrainBreakdown")) { out = IncidentType::TRAIN_BREAKDOWN; return true; }
    if (equalsIgnoreCase(s, "HeavyLoad"))      { out = IncidentType::HEAVY_LOAD;      return true; }
    if (equalsIgnoreCase(s, "Emergency"))      { out = IncidentType::EMERGENCY;       return true; }
    if (equalsIgnoreCase(s, "Weather"))        { out = IncidentType::WEATHER;         return true; }
    if (equalsIgnoreCase(s, "Other"))          { out = IncidentType::OTHER;           return true; }
    return false;
}

bool parseSeverity(const std::string& s, Severity& out) {
    if (equalsIgnoreCase(s, "Minor"))    { out = Severity::MINOR;    return true; }
    if (equalsIgnoreCase(s, "Moderate")) { out = Severity::MODERATE; return true; }
    if (equalsIgnoreCase(s, "Major"))    { out = Severity::MAJOR;    return true; }
    if (equalsIgnoreCase(s, "Critical")) { out = Severity::CRITICAL; return true; }
    return false;
}

} // namespace

MetroService::MetroService(MetroNetwork& net, DelayService& delaySvc, ICardRepository& cardRepository,
                            CachedShortestTimeStrategy& router, Turnstile& gate)
    : network(net), delayService(delaySvc), cardRepo(cardRepository), cachedRouter(router), turnstile(gate) {}

int MetroService::findDirectLine(int stationA, int stationB) const {
    for (const auto& edge : network.getNeighbors(stationA)) {
        if (edge.to == stationB) return edge.lineId;
    }
    return -1;
}

// ---------------- Ticketing ----------------

CardInfoResult MetroService::createOrRechargeCard(const std::string& cardId, const std::string& cardTypeStr, double amount) {
    CardInfoResult result;
    result.cardId = cardId;

    if (amount <= 0) {
        result.message = "Amount must be a positive number.";
        return result;
    }

    if (!cardRepo.exists(cardId)) {
        CardType type = parseCardType(cardTypeStr);
        cardRepo.create(cardId, type, amount);

        result.success = true;
        result.created = true;
        result.cardType = cardTypeToString(type);
        result.balance = amount;
        result.message = "New " + result.cardType + " card created.";
    } else {
        cardRepo.recharge(cardId, amount);

        CardRecord rec;
        cardRepo.find(cardId, rec);
        result.success = true;
        result.created = false;
        result.cardType = cardTypeToString(rec.cardType);
        result.balance = rec.balance;
        result.message = "Card recharged.";
    }
    return result;
}

CardInfoResult MetroService::getCardInfo(const std::string& cardId) {
    CardInfoResult result;
    result.cardId = cardId;

    CardRecord rec;
    if (!cardRepo.find(cardId, rec)) {
        result.message = "Card not found.";
        return result;
    }

    result.success = true;
    result.cardType = cardTypeToString(rec.cardType);
    result.balance = rec.balance;
    result.checkedIn = rec.entryStationId != -1;
    if (result.checkedIn) {
        result.checkedInStation = network.getStationName(rec.entryStationId);
    }
    result.message = "OK";
    return result;
}

// ---------------- Routing ----------------

RouteSearchResult MetroService::searchRoute(const std::string& startStationName, const std::string& endStationName, const std::string& strategy) {
    RouteSearchResult result;

    int startId = network.getStationId(startStationName);
    int endId = network.getStationId(endStationName);
    if (startId == -1 || endId == -1) {
        result.message = "One or both stations not found in the network.";
        return result;
    }

    RouteResult route;
    if (equalsIgnoreCase(strategy, "fewest")) {
        FewestStationsStrategy bfs;
        route = bfs.findPath(network, delayService, startId, endId);
        result.isTimeMetric = false;
    } else {
        // Default: fastest (Dijkstra, cached, interchange-penalty-aware)
        route = cachedRouter.findPath(network, delayService, startId, endId);
        result.isTimeMetric = true;
    }

    if (route.path.empty()) {
        result.message = "No path exists between these stations (a track on every route may be closed).";
        return result;
    }

    result.success = true;
    result.metric = route.totalMetric;
    for (int stationId : route.path) {
        result.stationNames.push_back(network.getStationName(stationId));
    }
    result.message = "OK";
    return result;
}

// ---------------- Turnstile ----------------

TapOutcome MetroService::tapIn(const std::string& cardId, const std::string& stationName) {
    int stationId = network.getStationId(stationName);
    if (stationId == -1) {
        TapOutcome outcome;
        outcome.message = "Station not found.";
        return outcome;
    }
    return turnstile.tapCard(cardId, stationId, false);
}

TapOutcome MetroService::tapOut(const std::string& cardId, const std::string& stationName) {
    int stationId = network.getStationId(stationName);
    if (stationId == -1) {
        TapOutcome outcome;
        outcome.isCheckout = true;
        outcome.message = "Station not found.";
        return outcome;
    }
    return turnstile.tapCard(cardId, stationId, true);
}

SimpleResult MetroService::setTurnstileOutOfOrder() {
    StateActionResult r = turnstile.triggerMaintenance();
    return { r.success, r.message };
}

SimpleResult MetroService::repairTurnstile() {
    StateActionResult r = turnstile.repairGate();
    return { r.success, r.message };
}

std::string MetroService::getTurnstileStatus() const {
    return turnstile.getStateName();
}

// ---------------- Admin: incidents ----------------

IncidentActionResult MetroService::reportIncident(const std::string& fromStationName, const std::string& toStationName,
                                                    const std::string& typeStr, const std::string& severityStr,
                                                    int durationMinutes, const std::string& reportedBy) {
    IncidentActionResult result;

    int fromId = network.getStationId(fromStationName);
    int toId = network.getStationId(toStationName);
    if (fromId == -1 || toId == -1) {
        result.message = "One or both stations not found.";
        return result;
    }

    int lineId = findDirectLine(fromId, toId);
    if (lineId == -1) {
        result.message = "No direct track exists between these two stations.";
        return result;
    }

    if (durationMinutes <= 0) {
        result.message = "Duration must be a positive number of minutes.";
        return result;
    }

    IncidentType type;
    if (!parseIncidentType(typeStr, type)) {
        result.message = "Unrecognized incident type.";
        return result;
    }
    Severity severity;
    if (!parseSeverity(severityStr, severity)) {
        result.message = "Unrecognized severity level.";
        return result;
    }

    int eventId = delayService.reportIncident(fromId, toId, lineId, type, severity, durationMinutes, reportedBy);

    result.success = true;
    result.eventId = eventId;
    result.lineName = network.getLineName(lineId);
    result.message = "Incident #" + std::to_string(eventId) + " logged on the " + result.lineName + " Line.";
    return result;
}

SimpleResult MetroService::removeIncident(int eventId) {
    if (delayService.removeIncident(eventId)) {
        cachedRouter.invalidateCache();
        return { true, "Incident removed." };
    }
    return { false, "Incident not found or already inactive." };
}

IncidentActionResult MetroService::closeTrack(const std::string& fromStationName, const std::string& toStationName, int durationMinutes) {
    IncidentActionResult result;

    int fromId = network.getStationId(fromStationName);
    int toId = network.getStationId(toStationName);
    if (fromId == -1 || toId == -1) {
        result.message = "One or both stations not found.";
        return result;
    }

    int lineId = findDirectLine(fromId, toId);
    if (lineId == -1) {
        result.message = "No direct track exists between these two stations.";
        return result;
    }

    if (durationMinutes <= 0) {
        result.message = "Duration must be a positive number of minutes.";
        return result;
    }

    delayService.closeTrack(fromId, toId, lineId, IncidentType::MAINTENANCE, durationMinutes, "Admin");

    result.success = true;
    result.lineName = network.getLineName(lineId);
    result.message = "Track closed on the " + result.lineName + " Line.";
    return result;
}

SimpleResult MetroService::reopenTrack(const std::string& fromStationName, const std::string& toStationName) {
    int fromId = network.getStationId(fromStationName);
    int toId = network.getStationId(toStationName);
    if (fromId == -1 || toId == -1) {
        return { false, "One or both stations not found." };
    }

    delayService.reopenTrack(fromId, toId);
    cachedRouter.invalidateCache();
    return { true, "Track reopened." };
}

std::vector<IncidentSummary> MetroService::getActiveIncidents() const {
    std::vector<IncidentSummary> result;
    for (const auto& ev : delayService.getActiveIncidents()) {
        IncidentSummary summary;
        summary.eventId = ev.eventId;
        summary.fromStationName = network.getStationName(ev.fromStation);
        summary.toStationName = network.getStationName(ev.toStation);
        summary.lineName = network.getLineName(ev.lineId);
        summary.incidentType = DelayService::getTypeString(ev.type);
        summary.severity = DelayService::getSeverityString(ev.severity);
        summary.delayMinutes = ev.delayMinutes;
        summary.trackClosed = ev.trackClosed;
        summary.reportedBy = ev.reportedBy;
        result.push_back(summary);
    }
    return result;
}

// ---------------- Observer subscriptions ----------------

SimpleResult MetroService::subscribeToLine(const std::string& cardId, const std::string& lineName) {
    if (!cardRepo.exists(cardId)) {
        return { false, "Card not found. Create it first." };
    }

    int lineId = network.getLineId(lineName);
    if (lineId == -1) {
        return { false, "Line not found." };
    }

    if (subscribers.find(cardId) == subscribers.end()) {
        subscribers[cardId] = std::make_unique<CardIncidentSubscriber>(cardId);
    }
    delayService.subscribeToLine(lineId, subscribers[cardId].get());

    return { true, "Card " + cardId + " is now subscribed to " + lineName + " Line alerts." };
}