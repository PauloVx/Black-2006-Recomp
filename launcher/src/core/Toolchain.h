#pragma once

#include "core/ProjectLayout.h"

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace black::core {

/// A program the game's build needs (or, marked not required, one that only helps).
struct Tool {
    std::string key;   // "cpp", "cmake", "python", "rust", "git"...
    std::string name;  // shown to the player, in English capitals
    std::string help;
    bool required = true;
    bool found = false;
    std::string version{};
    std::filesystem::path program{};  // what to run; the installation folder for Visual Studio
    std::string package{};            // Windows: the winget package that installs it
    std::vector<std::string> installArguments{};
};

struct Toolchain {
    std::vector<Tool> tools;
    /// Windows: winget is there to install what is missing.
    bool canInstall = false;
    /// Folders of the tools found, searched first by the build.
    std::vector<std::filesystem::path> folders;

    [[nodiscard]] bool ready() const;
    [[nodiscard]] const Tool* find(std::string_view key) const;
    /// PATH for the build's processes: the tools' folders, then the system's current PATH (which an installer
    /// may have changed since the launcher started), then the launcher's own.
    [[nodiscard]] std::string pathVariable() const;
};

/// Looks for each tool on PATH and where its installer puts it. Runs each one briefly to read its version.
[[nodiscard]] Toolchain detectToolchain(const ProjectLayout& project);

struct InstallCommand {
    std::string tool;  // the Tool's name
    std::filesystem::path program;
    std::vector<std::string> arguments;
};
/// winget commands for the required tools that are missing (none when winget is not there, or off Windows).
[[nodiscard]] std::vector<InstallCommand> installCommands(const Toolchain& toolchain);
/// Linux: the command that installs what is missing, for the player to run.
[[nodiscard]] std::string installHint(const Toolchain& toolchain);

/// The first "major.minor[.patch]" in a program's version text.
[[nodiscard]] std::optional<std::array<int, 3>> parseVersion(std::string_view text);
[[nodiscard]] bool atLeast(const std::array<int, 3>& version, int major, int minor);

/// A program's output (stdout and stderr) when it exits with 0.
[[nodiscard]] std::optional<std::string> captureOutput(const std::filesystem::path& program,
                                                       const std::vector<std::string>& arguments);

}  // namespace black::core
