#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace black::core {

/// SHA-256 of the one retail Xbox default.xbe this project supports (also checked by scripts/setup.ps1).
inline constexpr std::string_view kSupportedXbeSha256 =
    "DF2739C372D254A90AEECCF5971D12097F22D89FBB90DB021FDA96DD4DEB68F6";

enum class GameFilesState {
    Missing,             ///< No default.xbe yet: the player has not chosen their disc.
    Unreadable,          ///< default.xbe exists but could not be read.
    UnsupportedVersion,  ///< A different release (or a modified file): the build would not work.
    Supported,           ///< The supported retail release.
};

struct GameFilesStatus {
    GameFilesState state = GameFilesState::Missing;
    /// The file's SHA-256 when it could be read, for support questions.
    std::string sha256;
    /// Why it was unreadable, when it was.
    std::string error;
};

/// Checks an extracted disc's default.xbe against the supported release.
[[nodiscard]] GameFilesStatus inspectGameFiles(const std::filesystem::path& defaultXbe);

}  // namespace black::core
