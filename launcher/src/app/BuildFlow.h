#pragma once

#include "app/BackgroundTask.h"
#include "app/MenuFlow.h"
#include "core/GameBuild.h"
#include "core/ProjectLayout.h"
#include "core/Toolchain.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace black::app {

class MenuFont;

/// Building the game from the player's disc: the tools it needs (installed with winget when missing), then the
/// build itself, with each stage's progress, and what went wrong if it fails.
class BuildFlow final : public MenuFlow {
public:
    BuildFlow(core::ProjectLayout project, const MenuFont* rows);

    void update(double time, std::size_t selectedRow) override;
    [[nodiscard]] Page page() override;
    Outcome handle(MenuEvent event) override;
    Outcome back() override;
    [[nodiscard]] bool busy() const override { return false; }
    [[nodiscard]] bool canLeave() const override {
        return m_screen != Screen::Installing && m_screen != Screen::Building;
    }

private:
    enum class Screen { Checking, Tools, Installing, Building, Finished, Failed };
    enum class Action { Install, Build, BuildFromScratch, CheckAgain, Stop, Continue, TryAgain, OpenLog, Back };

    struct InstallProgress {
        std::size_t index = 0;
        std::size_t count = 0;
        std::string tool;
        std::string line;
    };
    struct InstallResult {
        std::vector<std::string> failed;
        bool stopped = false;
    };

    void detect();
    void install();
    void build(bool fromScratch);

    float drawTools(ImDrawList& list, ImVec2 at, float width) const;
    float drawInstalling(ImDrawList& list, ImVec2 at, float width) const;
    float drawBuilding(ImDrawList& list, ImVec2 at, float width) const;
    float drawOutcome(ImDrawList& list, ImVec2 at, float width) const;

    core::ProjectLayout m_project;
    const MenuFont* m_rowFont;
    Screen m_screen = Screen::Checking;
    std::vector<Action> m_actions;
    double m_time = 0;
    std::string m_notice;
    bool m_noticeIsProblem = false;

    std::optional<core::Toolchain> m_toolchain;
    std::optional<core::BuildOutcome> m_outcome;
    std::chrono::steady_clock::time_point m_started;
    std::chrono::steady_clock::duration m_took{};
    BackgroundTask<core::Toolchain, int> m_detect;
    BackgroundTask<InstallResult, InstallProgress> m_install;
    BackgroundTask<core::BuildOutcome, core::BuildProgress> m_build;
};

}  // namespace black::app
