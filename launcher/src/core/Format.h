#pragma once

#include <cstdint>
#include <string>

namespace black::core {

/// "6.8 GB", "512 MB", "12 KB", "0 B".
[[nodiscard]] std::string formatBytes(std::uint64_t bytes);
[[nodiscard]] std::string formatRate(double bytesPerSecond);
/// "ABOUT 2 MIN LEFT", "LESS THAN A MINUTE LEFT", "ABOUT 1 H 05 MIN LEFT".
[[nodiscard]] std::string formatTimeLeft(double seconds);

}  // namespace black::core
