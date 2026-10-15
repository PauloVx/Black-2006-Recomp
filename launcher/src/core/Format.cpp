#include "core/Format.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace black::core {

std::string formatBytes(std::uint64_t bytes) {
    constexpr std::array<const char*, 4> kUnits = {"KB", "MB", "GB", "TB"};
    if (bytes < 1024) {
        return std::to_string(bytes) + " B";
    }
    auto value = static_cast<double>(bytes) / 1024.0;
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < kUnits.size()) {
        value /= 1024.0;
        ++unit;
    }
    std::array<char, 32> text{};
    std::snprintf(text.data(), text.size(), unit >= 2 ? "%.1f %s" : "%.0f %s", value, kUnits[unit]);
    return text.data();
}

std::string formatRate(double bytesPerSecond) {
    if (!(bytesPerSecond > 0) || !std::isfinite(bytesPerSecond)) {
        return "-";
    }
    return formatBytes(static_cast<std::uint64_t>(bytesPerSecond)) + "/S";
}

std::string formatTimeLeft(double seconds) {
    if (!(seconds >= 0) || !std::isfinite(seconds)) {
        return "";
    }
    if (seconds < 60.0) {
        return "LESS THAN A MINUTE LEFT";
    }
    const auto minutes = static_cast<long long>(std::lround(seconds / 60.0));
    if (minutes < 60) {
        return "ABOUT " + std::to_string(minutes) + " MIN LEFT";
    }
    std::array<char, 48> text{};
    std::snprintf(text.data(), text.size(), "ABOUT %lld H %02lld MIN LEFT", minutes / 60, minutes % 60);
    return text.data();
}

}  // namespace black::core
