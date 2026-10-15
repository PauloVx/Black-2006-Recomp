#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace black::core {

class ProjectLayout {
public:
    explicit ProjectLayout(std::filesystem::path root);

    /// The project folder containing `start` (or `start` itself), searching upwards. A project folder has
    /// CMakeLists.txt and scripts/generate.py.
    [[nodiscard]] static std::optional<ProjectLayout> locate(const std::filesystem::path& start);
    [[nodiscard]] static bool isProjectRoot(const std::filesystem::path& folder);

    [[nodiscard]] const std::filesystem::path& root() const noexcept { return m_root; }
    [[nodiscard]] std::filesystem::path gameDirectory() const { return m_root / "game"; }
    [[nodiscard]] std::filesystem::path defaultXbe() const { return gameDirectory() / "default.xbe"; }
    [[nodiscard]] std::filesystem::path localDirectory() const { return m_root / "local"; }
    /// An optional replacement for the built-in logo (git-ignored, like all of local/).
    [[nodiscard]] std::filesystem::path customLogo() const { return localDirectory() / "logo.png"; }

    /// Where a build leaves the game, most preferred first: a portable release's bin/, then the Windows build tree.
    [[nodiscard]] std::vector<std::filesystem::path> gameExecutableCandidates() const;
    [[nodiscard]] std::optional<std::filesystem::path> gameExecutable() const;

private:
    std::filesystem::path m_root;
};

}  // namespace black::core
