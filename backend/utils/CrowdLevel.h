#pragma once
#include <string>

namespace utils {

enum class CrowdLevel { GREEN, YELLOW, RED };

inline CrowdLevel classifyCrowd(int queueCount) {
    if (queueCount < 5)  return CrowdLevel::GREEN;
    if (queueCount <= 15) return CrowdLevel::YELLOW;
    return CrowdLevel::RED;
}

inline std::string crowdLevelStr(CrowdLevel level) {
    switch (level) {
        case CrowdLevel::GREEN:  return "GREEN";
        case CrowdLevel::YELLOW: return "YELLOW";
        case CrowdLevel::RED:    return "RED";
    }
    return "GREEN";
}

} // namespace utils
