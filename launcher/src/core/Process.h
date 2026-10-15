#pragma once

#include "core/Result.h"

#include <filesystem>
#include <string>
#include <vector>

namespace black::core {

/// Starts a program that keeps running on its own (the launcher does not wait for it).
/// Returns true once it has started.
[[nodiscard]] Result<bool> startDetached(const std::filesystem::path& program,
                                         const std::vector<std::string>& arguments,
                                         const std::filesystem::path& workingDirectory);

/// Opens a folder in the system's file manager.
[[nodiscard]] Result<bool> openFolder(const std::filesystem::path& folder);

}  // namespace black::core
