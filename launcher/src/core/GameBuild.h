#pragma once

#include "core/ProjectLayout.h"
#include "core/Toolchain.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace black::core {

/// Turning the player's disc into the PC game, in the order it happens.
enum class BuildStage { Prepare, Generate, Configure, Compile, Languages };
inline constexpr std::size_t kBuildStages = 5;

struct BuildProgress {
    BuildStage stage = BuildStage::Prepare;
    std::optional<float> fraction;  // of the stage, when it can be told
    std::string activity;           // the latest line of output
    std::size_t compiled = 0;       // source files compiled so far
    std::array<bool, kBuildStages> skipped{};
};

struct BuildOptions {
    /// Translate the code from the disc again even when it is up to date.
    bool fromScratch = false;
};

struct BuildOutcome {
    bool ok = false;
    bool stopped = false;
    BuildStage stage = BuildStage::Prepare;  // where it failed or stopped
    std::string message;
    std::string warning;  // the game was built, but something else could not be done
    std::filesystem::path log;
    std::vector<std::string> lastLines;
};

/// Runs every stage, logging all output to reports/build-<time>.log. Stopping (the token) ends the step in
/// progress and everything it started; building again picks up from there: code already translated is kept, and
/// the compiler only redoes what changed.
[[nodiscard]] BuildOutcome buildGame(const ProjectLayout& project, const Toolchain& toolchain,
                                     const BuildOptions& options, std::stop_token stop,
                                     const std::function<void(const BuildProgress&)>& report);

/// Whether src/recomp/gen holds code translated since the disc and the translator's inputs last changed.
[[nodiscard]] bool generatedCodeIsCurrent(const ProjectLayout& project);
/// generate.py's progress, from its "Running tools...." lines.
[[nodiscard]] std::optional<float> generateFraction(std::string_view line);
/// A file the compiler reports compiling (MSBuild echoes each source file's name).
[[nodiscard]] bool compiledFileLine(std::string_view line);
/// Ninja's "[12/345] ..." as 12/345.
[[nodiscard]] std::optional<float> ninjaFraction(std::string_view line);

}  // namespace black::core
