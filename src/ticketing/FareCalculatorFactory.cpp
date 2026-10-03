#include "../../include/ticketing/FareCalculatorFactory.h"

std::unique_ptr<FarePolicy> FareCalculatorFactory::createPolicy(CardType type) {
    switch (type) {
        case CardType::STUDENT:        return std::make_unique<StudentFarePolicy>();
        case CardType::SENIOR_CITIZEN: return std::make_unique<SeniorCitizenFarePolicy>();
        case CardType::DAILY_PASS:     return std::make_unique<DailyPassFarePolicy>();
        case CardType::REGULAR:
        default:                       return std::make_unique<RegularFarePolicy>();
    }
}