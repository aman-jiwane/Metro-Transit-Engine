#pragma once
#include <string>

class Station {
public:
    int id;
    std::string name;
    bool isInterchange;

    Station();

    Station(int id, std::string name, bool interchange = false);
};