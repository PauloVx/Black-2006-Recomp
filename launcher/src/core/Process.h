#pragma once

#include "core/Result.h"

#include <filesystem>
#include <string>
#include <vector>

namespace black::core {

[[nodiscard]] Result<bool> startDetached(const std::filesystem::path& program,
                                         const std::vector<std::string>& arguments,
                                         const std::filesystem::path& workingDirectory);

[[nodiscard]] Result<bool> openFolder(const std::filesystem::path& folder);

}  // namespace black::core
