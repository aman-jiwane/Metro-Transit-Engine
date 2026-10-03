#pragma once
#include <memory>
#include "../models/CardType.h"
#include "FarePolicy.h"

class FareCalculatorFactory {
public:
    static std::unique_ptr<FarePolicy> createPolicy(CardType type);
};