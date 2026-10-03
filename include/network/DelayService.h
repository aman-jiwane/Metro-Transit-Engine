#pragma once
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include "../models/DelayEvent.h"
#include "IIncidentSubscriber.h"

class DelayService {
private:
    std::vector<DelayEvent> activeIncidents;
    mutable std::shared_mutex rw_lock;
    int nextEventId = 1;

    // Observer registries. lineSubscribers holds commuters who only want to
    // hear about their specific line; globalSubscribers hears about every
    // incident regardless of line (used by the route cache, which needs to
    // invalidate on any change anywhere in the network).
    std::unordered_map<int, std::vector<IIncidentSubscriber*>> lineSubscribers;
    std::vector<IIncidentSubscriber*> globalSubscribers;

    void cleanExpiredEvents() const;

    // Notifies subscribers for event.lineId plus every global subscriber.
    // Deliberately takes no lock itself beyond a brief one to copy the
    // subscriber list - calling into subscriber code (a virtual function
    // this class doesn't control) while holding rw_lock would risk
    // deadlock if a subscriber ever called back into DelayService.
    void notifySubscribers(const DelayEvent& event);

public:
    // Public for the same reason as MetroNetwork's constructor: unit tests
    // need isolated instances rather than shared getInstance() state.
    DelayService() {}

    static DelayService& getInstance();
    DelayService(const DelayService&) = delete;
    DelayService& operator=(const DelayService&) = delete;

    int reportIncident(int from, int to, int lineId, IncidentType type, Severity severity, int durationMinutes, std::string reporter);
    bool removeIncident(int eventId);
    void closeTrack(int from, int to, int lineId, IncidentType type, int durationMinutes, std::string reporter);
    void reopenTrack(int from, int to);

    std::vector<DelayEvent> getActiveIncidents() const;
    double getDelay(int from, int to) const;
    bool isTrackClosed(int from, int to) const;

    void subscribeToLine(int lineId, IIncidentSubscriber* subscriber);
    void unsubscribeFromLine(int lineId, IIncidentSubscriber* subscriber);
    void subscribeGlobal(IIncidentSubscriber* subscriber);
    void unsubscribeGlobal(IIncidentSubscriber* subscriber);

    static std::string getSeverityString(Severity s);
    static std::string getTypeString(IncidentType t);
};