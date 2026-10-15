#pragma once

#include "core/Result.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace black::core {

[[nodiscard]] Result<std::vector<std::uint8_t>> readFile(const std::filesystem::path& path);

}  // namespace black::core
