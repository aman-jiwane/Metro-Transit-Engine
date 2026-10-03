#include "../../include/models/Station.h"

Station::Station() : id(-1), name(""), isInterchange(false) {}

Station::Station(int id, std::string name, bool interchange) 
    : id(id), name(name), isInterchange(interchange) {}