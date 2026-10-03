#include "../../include/network/DelayService.h"
#include <algorithm>
#include <iostream>
#include <mutex>
#include <shared_mutex>

DelayService& DelayService::getInstance() {
    static DelayService instance;
    return instance;
}

void DelayService::cleanExpiredEvents() const {
    auto now = std::chrono::system_clock::now();
    auto& incidents = const_cast<std::vector<DelayEvent>&>(activeIncidents);
    for (auto& event : incidents) {
        if (event.active && now > event.endTime) {
            event.active = false;
        }
    }
}

void DelayService::notifySubscribers(const DelayEvent& event) {
    std::vector<IIncidentSubscriber*> toNotify;
    {
        std::shared_lock<std::shared_mutex> lock(rw_lock);
        auto it = lineSubscribers.find(event.lineId);
        if (it != lineSubscribers.end()) {
            toNotify.insert(toNotify.end(), it->second.begin(), it->second.end());
        }
        toNotify.insert(toNotify.end(), globalSubscribers.begin(), globalSubscribers.end());
    } // lock released before calling into subscriber code

    for (auto* sub : toNotify) {
        sub->onIncidentReported(event);
    }
}

int DelayService::reportIncident(int from, int to, int lineId, IncidentType type, Severity severity, int durationMinutes, std::string reporter) {
    DelayEvent event;
    {
        std::unique_lock<std::shared_mutex> lock(rw_lock);

        event.eventId = nextEventId++;
        event.fromStation = from;
        event.toStation = to;
        event.lineId = lineId;
        event.type = type;
        event.severity = severity;
        event.trackClosed = (severity == Severity::TRACK_CLOSED);
        event.active = true;
        event.startTime = std::chrono::system_clock::now();
        event.endTime = event.startTime + std::chrono::minutes(durationMinutes);
        event.reportedBy = reporter;

        switch (severity) {
            case Severity::MINOR: event.delayMinutes = 2.0; break;
            case Severity::MODERATE: event.delayMinutes = 5.0; break;
            case Severity::MAJOR: event.delayMinutes = 10.0; break;
            case Severity::CRITICAL: event.delayMinutes = 20.0; break;
            case Severity::TRACK_CLOSED: event.delayMinutes = 0.0; break;
        }

        activeIncidents.push_back(event);
    } // lock released before notifying

    notifySubscribers(event);
    return event.eventId;
}

bool DelayService::removeIncident(int eventId) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);
    for (auto& event : activeIncidents) {
        if (event.eventId == eventId && event.active) {
            event.active = false;
            return true;
        }
    }
    return false;
}

void DelayService::closeTrack(int from, int to, int lineId, IncidentType type, int durationMinutes, std::string reporter) {
    reportIncident(from, to, lineId, type, Severity::TRACK_CLOSED, durationMinutes, reporter);
}

void DelayService::reopenTrack(int from, int to) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);
    for (auto& event : activeIncidents) {
        if (event.active && event.trackClosed &&
            ((event.fromStation == from && event.toStation == to) ||
             (event.fromStation == to && event.toStation == from))) {
            event.active = false;
        }
    }
}

std::vector<DelayEvent> DelayService::getActiveIncidents() const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    cleanExpiredEvents();

    std::vector<DelayEvent> active;
    for (const auto& event : activeIncidents) {
        if (event.active) active.push_back(event);
    }
    return active;
}

double DelayService::getDelay(int from, int to) const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    cleanExpiredEvents();

    double totalDelay = 0.0;
    for (const auto& event : activeIncidents) {
        if (event.active && !event.trackClosed &&
            ((event.fromStation == from && event.toStation == to) ||
             (event.fromStation == to && event.toStation == from))) {
            totalDelay += event.delayMinutes;
        }
    }
    return totalDelay;
}

bool DelayService::isTrackClosed(int from, int to) const {
    std::shared_lock<std::shared_mutex> lock(rw_lock);
    cleanExpiredEvents();

    for (const auto& event : activeIncidents) {
        if (event.active && event.trackClosed &&
            ((event.fromStation == from && event.toStation == to) ||
             (event.fromStation == to && event.toStation == from))) {
            return true;
        }
    }
    return false;
}

void DelayService::subscribeToLine(int lineId, IIncidentSubscriber* subscriber) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);
    lineSubscribers[lineId].push_back(subscriber);
}

void DelayService::unsubscribeFromLine(int lineId, IIncidentSubscriber* subscriber) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);
    auto it = lineSubscribers.find(lineId);
    if (it != lineSubscribers.end()) {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), subscriber), vec.end());
    }
}

void DelayService::subscribeGlobal(IIncidentSubscriber* subscriber) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);
    globalSubscribers.push_back(subscriber);
}

void DelayService::unsubscribeGlobal(IIncidentSubscriber* subscriber) {
    std::unique_lock<std::shared_mutex> lock(rw_lock);
    globalSubscribers.erase(std::remove(globalSubscribers.begin(), globalSubscribers.end(), subscriber), globalSubscribers.end());
}

std::string DelayService::getSeverityString(Severity s) {
    switch (s) {
        case Severity::MINOR: return "Minor";
        case Severity::MODERATE: return "Moderate";
        case Severity::MAJOR: return "Major";
        case Severity::CRITICAL: return "Critical";
        case Severity::TRACK_CLOSED: return "Track Closed";
        default: return "Unknown";
    }
}

std::string DelayService::getTypeString(IncidentType t) {
    switch (t) {
        case IncidentType::MAINTENANCE: return "Maintenance";
        case IncidentType::SIGNAL_FAILURE: return "Signal Failure";
        case IncidentType::TRAIN_BREAKDOWN: return "Train Breakdown";
        case IncidentType::HEAVY_LOAD: return "Heavy Passenger Load";
        case IncidentType::EMERGENCY: return "Emergency";
        case IncidentType::WEATHER: return "Weather";
        default: return "Other";
    }
}