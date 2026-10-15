#include "core/ProjectLayout.h"

#include <system_error>
#include <utility>

namespace black::core {
namespace fs = std::filesystem;

ProjectLayout::ProjectLayout(fs::path root) : m_root(std::move(root)) {}

bool ProjectLayout::isProjectRoot(const fs::path& folder) {
    std::error_code error;
    return fs::is_regular_file(folder / "CMakeLists.txt", error) &&
           fs::is_regular_file(folder / "scripts" / "generate.py", error);
}

std::optional<ProjectLayout> ProjectLayout::locate(const fs::path& start) {
    std::error_code error;
    fs::path folder = fs::weakly_canonical(start, error);
    if (error) {
        folder = start;
    }
    while (!folder.empty()) {
        if (isProjectRoot(folder)) {
            return ProjectLayout(folder);
        }
        fs::path parent = folder.parent_path();
        if (parent == folder) {
            break;
        }
        folder = std::move(parent);
    }
    return std::nullopt;
}

std::vector<fs::path> ProjectLayout::gameExecutableCandidates() const {
    // The game is a Windows program on every host: Linux runs it through Wine/Proton.
    return {m_root / "bin" / "black_xbox_recomp.exe", m_root / "build-vs2022" / "Release" / "black_xbox_recomp.exe",
            m_root / "build-linux-wine" / "black_xbox_recomp.exe"};
}

std::optional<fs::path> ProjectLayout::gameExecutable() const {
    std::error_code error;
    for (fs::path& candidate : gameExecutableCandidates()) {
        if (fs::is_regular_file(candidate, error)) {
            return std::move(candidate);
        }
    }
    return std::nullopt;
}

}  // namespace black::core
