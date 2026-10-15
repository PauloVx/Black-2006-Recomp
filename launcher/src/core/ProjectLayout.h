#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace black::core {

/// Where things live in a Black-2006-Recomp checkout or release folder.
class ProjectLayout {
public:
    explicit ProjectLayout(std::filesystem::path root);

    /// The project folder containing `start` (or `start` itself), searching upwards. A project folder has
    /// CMakeLists.txt and scripts/generate.py.
    [[nodiscard]] static std::optional<ProjectLayout> locate(const std::filesystem::path& start);
    [[nodiscard]] static bool isProjectRoot(const std::filesystem::path& folder);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return m_root; }
    /// The extracted disc: default.xbe, levels/, sound/ ...
    [[nodiscard]] std::filesystem::path gameDirectory() const { return m_root / "game"; }
    [[nodiscard]] std::filesystem::path defaultXbe() const { return gameDirectory() / "default.xbe"; }
    /// Player settings shared with the game (video.ini, controls.ini, menus.ini).
    [[nodiscard]] std::filesystem::path localDirectory() const { return m_root / "local"; }
    /// An optional logo image the player supplies (PNG/JPEG with alpha), used in place of the disc's small one.
    /// It lives in local/, which Git ignores: logo artwork is not part of the project.
    [[nodiscard]] std::filesystem::path customLogo() const { return localDirectory() / "logo.png"; }

    /// Where a build leaves the game, most preferred first: a portable release's bin/, then the Windows build tree.
    [[nodiscard]] std::vector<std::filesystem::path> gameExecutableCandidates() const;
    /// The first candidate that exists, if the game has been built.
    [[nodiscard]] std::optional<std::filesystem::path> gameExecutable() const;

    /// The previous launcher's entry points (BLACK PC Launcher / Settings on Windows, the .sh scripts on Linux),
    /// which start the game with the player's settings until this launcher takes that over.
    [[nodiscard]] std::filesystem::path legacyPlay() const;
    [[nodiscard]] std::filesystem::path legacySettings() const;

private:
    std::filesystem::path m_root;
};

}  // namespace black::core
