#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace black::core {

/// SHA-256 of the one retail Xbox default.xbe this project supports (also checked by scripts/setup.ps1).
inline constexpr std::string_view kSupportedXbeSha256 =
    "DF2739C372D254A90AEECCF5971D12097F22D89FBB90DB021FDA96DD4DEB68F6";

enum class GameFilesState {
    Missing,
    Unreadable,
    UnsupportedVersion,
    Supported,
};

struct GameFilesStatus {
    GameFilesState state = GameFilesState::Missing;
    std::string sha256;
    std::string error;
};

[[nodiscard]] GameFilesStatus inspectGameFiles(const std::filesystem::path& defaultXbe);

}  // namespace black::core
