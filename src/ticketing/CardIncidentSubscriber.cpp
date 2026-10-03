#include "../../include/ticketing/CardIncidentSubscriber.h"
#include "../../include/network/DelayService.h"
#include "../../include/network/MetroNetwork.h"
#include <iostream>

CardIncidentSubscriber::CardIncidentSubscriber(std::string cardId) : cardId(std::move(cardId)) {}

void CardIncidentSubscriber::onIncidentReported(const DelayEvent& event) {
    const MetroNetwork& network = MetroNetwork::getInstance();
    std::cout << "  [Notification -> " << cardId << "] "
              << network.getLineName(event.lineId) << " Line: "
              << network.getStationName(event.fromStation) << " <-> "
              << network.getStationName(event.toStation) << " - "
              << DelayService::getTypeString(event.type) << " ("
              << DelayService::getSeverityString(event.severity) << ")";
    if (event.trackClosed) {
        std::cout << " - track closed.\n";
    } else {
        std::cout << ", +" << event.delayMinutes << " min.\n";
    }
}

const std::string& CardIncidentSubscriber::getCardId() const {
    return cardId;
}