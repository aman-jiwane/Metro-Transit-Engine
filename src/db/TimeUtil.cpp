#include "../../include/db/TimeUtil.h"
#include <ctime>

std::string nowIso8601() {
    std::time_t t = std::time(nullptr);
    std::tm tmVal = *std::gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmVal);
    return std::string(buf);
}