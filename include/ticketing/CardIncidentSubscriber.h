#pragma once
#include "../network/IIncidentSubscriber.h"
#include <string>

// A concrete Observer representing one commuter's card. In a real system
// this would push to a phone; here it prints to the console, simulating
// what that commuter would see.
class CardIncidentSubscriber : public IIncidentSubscriber {
private:
    std::string cardId;

public:
    explicit CardIncidentSubscriber(std::string cardId);
    void onIncidentReported(const DelayEvent& event) override;
    const std::string& getCardId() const;
};