#include "app/BuildFlow.h"

#include "app/MenuFont.h"
#include "app/Theme.h"
#include "app/Translations.h"
#include "core/ChildProcess.h"
#include "core/Process.h"
#include "core/Text.h"

#include <array>
#include <cstdio>
#include <memory>
#include <mutex>

namespace black::app {
namespace {

constexpr float kText = 11.0f;
constexpr float kLine = 22.0f;

constexpr std::array<const char*, core::kBuildStages> kStageNames = {
    trMark("PREPARE THE TOOLS"), trMark("TRANSLATE THE CODE FROM YOUR DISC"), trMark("SET UP THE BUILD"),
    trMark("COMPILE THE GAME"), trMark("MAKE THE MENU LANGUAGES")};

/// core::buildGame's messages, translated where they are shown
[[maybe_unused]] constexpr std::array kBuildMessages = {
    trMark("Stopped."),
    trMark("Some of the tools the build needs are missing."),
    trMark("Could not fetch the project's missing parts (git submodule update)."),
    trMark("Could not set up Python for the build (.venv)."),
    trMark("Could not install the disassembler (capstone) for the build."),
    trMark("Translating the game's code from your disc failed."),
    trMark("Setting up the build (CMake) failed."),
    trMark("Compiling the game failed."),
    trMark("Building the game failed."),
};

/// core::detectToolchain's tool names
[[maybe_unused]] constexpr std::array kToolNames = {
    trMark("VISUAL STUDIO 2022 C++ TOOLS"),
    trMark("CMAKE"),
    trMark("PYTHON"),
    trMark("RUST"),
    trMark("GIT"),
    trMark("CLANG AND LLVM 18"),
    trMark("C AND C++ COMPILERS"),
    trMark("CMAKE AND NINJA"),
    trMark("WINE"),
};

void mark(ImDrawList& list, ImVec2 at, const ImVec4& color) {
    list.AddCircleFilled(ImVec2(at.x + 4.0f, at.y + 6.0f), 3.5f, toU32(color));
}

std::string clock(std::chrono::steady_clock::duration elapsed) {
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    std::array<char, 16> text{};
    std::snprintf(text.data(), text.size(), "%lld:%02lld", static_cast<long long>(seconds / 60),
                  static_cast<long long>(seconds % 60));
    return text.data();
}

/// The end of a long line, so the part that changes stays visible.
std::string fit(const MenuFont& font, std::string text, float size, float width) {
    if (font.width(text, size) <= width) {
        return text;
    }
    while (text.size() > 4 && font.width("..." + text, size) > width) {
        text.erase(0, 1);
    }
    return "..." + text;
}

}  // namespace

BuildFlow::BuildFlow(core::ProjectLayout project, const MenuFont* rows)
    : m_project(std::move(project)), m_rowFont(rows) {
    detect();
}

void BuildFlow::detect() {
    m_screen = Screen::Checking;
    m_toolchain.reset();
    const core::ProjectLayout project = m_project;
    m_detect.start([project](std::stop_token, const auto&) { return core::detectToolchain(project); });
}

void BuildFlow::install() {
    m_screen = Screen::Installing;
    m_notice.clear();
    const std::vector<core::InstallCommand> commands = core::installCommands(*m_toolchain);
    m_install.start(
        [commands](std::stop_token stop, const auto& report) {
            InstallResult result;
            for (std::size_t i = 0; i < commands.size(); ++i) {
                if (stop.stop_requested()) {  // an installer is never cut off halfway; the next ones are skipped
                    result.stopped = true;
                    break;
                }
                auto progress =
                    std::make_shared<InstallProgress>(InstallProgress{i, commands.size(), commands[i].tool, {}});
                auto mutex = std::make_shared<std::mutex>();
                report(*progress);
                core::ChildOptions options;
                options.program = commands[i].program;
                options.arguments = commands[i].arguments;
                options.onLine = [progress, mutex, &report](std::string_view line, bool) {
                    const std::scoped_lock lock(*mutex);
                    if (!line.empty()) {
                        progress->line = std::string(line);
                        report(*progress);
                    }
                };
                auto child = core::ChildProcess::start(std::move(options));
                if (!child || child.value()->wait() != 0) {
                    result.failed.push_back(commands[i].tool);
                }
            }
            return result;
        },
        InstallProgress{0, commands.size(), {}, {}});
}

void BuildFlow::build(bool fromScratch) {
    m_screen = Screen::Building;
    m_notice.clear();
    m_outcome.reset();
    m_started = std::chrono::steady_clock::now();
    const core::ProjectLayout project = m_project;
    const core::Toolchain toolchain = *m_toolchain;
    m_build.start([project, toolchain, fromScratch](std::stop_token stop, const auto& report) {
        return core::buildGame(project, toolchain, core::BuildOptions{fromScratch}, std::move(stop), report);
    });
}

void BuildFlow::update(double time, std::size_t /*selectedRow*/) {
    m_time = time;
    if (auto toolchain = m_detect.takeOutcome()) {
        m_toolchain = std::move(*toolchain);
        m_screen = Screen::Tools;
    }
    if (auto installed = m_install.takeOutcome()) {
        if (!installed->failed.empty()) {
            std::string names;
            for (const std::string& name : installed->failed) {
                names += (names.empty() ? "" : ", ") + tr(name);
            }
            m_notice = trf("These could not be installed: {0}. Try again, or install them yourself.", {names});
            m_noticeIsProblem = true;
        }
        detect();
    }
    if (auto outcome = m_build.takeOutcome()) {
        m_took = std::chrono::steady_clock::now() - m_started;
        m_screen = outcome->ok ? Screen::Finished : Screen::Failed;
        m_outcome = std::move(*outcome);
    }
}

float BuildFlow::drawTools(ImDrawList& list, ImVec2 at, float width) const {
    float y = at.y;
    y += drawWrapped(list, *m_rowFont, ImVec2(at.x, y),
                     tr("Your disc becomes the PC game with these free tools. The first build takes a while; later "
                        "ones only redo what changed."),
                     kText, width, toU32(colors::kRow)) +
         10.0f;
    for (const core::Tool& tool : m_toolchain->tools) {
        const ImVec4 color = tool.found ? colors::kGood : tool.required ? colors::kBad : colors::kMuted;
        mark(list, ImVec2(at.x, y), color);
        m_rowFont->draw(list, ImVec2(at.x + 18.0f, y), tr(tool.name), kText, toU32(colors::kText));
        const std::string state = tool.found      ? (tool.version.empty() ? tr("FOUND") : tool.version)
                                  : tool.required ? tr("MISSING")
                                                  : tr("NOT NEEDED");
        m_rowFont->draw(list, ImVec2(at.x + 360.0f, y), state, kText, toU32(color));
        y += kLine;
    }
    return y + 8.0f - at.y;
}

float BuildFlow::drawInstalling(ImDrawList& list, ImVec2 at, float width) const {
    const InstallProgress progress = m_install.progress();
    m_rowFont->draw(list, at,
                    progress.tool.empty()
                        ? tr("STARTING...")
                        : trf("INSTALLING {0} ({1} OF {2})",
                              {tr(progress.tool), std::to_string(progress.index + 1), std::to_string(progress.count)}),
                    kText, toU32(colors::kText));
    drawProgressBar(list, ImVec2(at.x, at.y + 26.0f), width, std::nullopt, m_time);
    m_rowFont->draw(list, ImVec2(at.x, at.y + 56.0f), fit(*m_rowFont, progress.line, kText, width), kText,
                    toU32(colors::kMuted));
    return 56.0f + kLine + 8.0f;
}

float BuildFlow::drawBuilding(ImDrawList& list, ImVec2 at, float width) const {
    const core::BuildProgress progress = m_build.progress();
    const auto current = static_cast<std::size_t>(progress.stage);
    float y = at.y;
    for (std::size_t i = 0; i < core::kBuildStages; ++i) {
        const bool done = i < current;
        const ImVec4 color = progress.skipped.at(i) || done ? colors::kGood
                             : i == current                 ? colors::kLogo
                                                            : colors::kMuted;
        mark(list, ImVec2(at.x, y), color);
        m_rowFont->draw(list, ImVec2(at.x + 18.0f, y), tr(kStageNames.at(i)), kText,
                        toU32(i == current ? colors::kText : colors::kRow));
        std::string state;
        if (progress.skipped.at(i)) {
            state = tr("UP TO DATE");
        } else if (done) {
            state = tr("DONE");
        } else if (i == current) {
            state = progress.fraction ? std::to_string(static_cast<int>(*progress.fraction * 100.0f)) + "%" : "...";
            if (i == static_cast<std::size_t>(core::BuildStage::Compile) && progress.compiled > 0) {
                state = trf("{0} FILES  {1}", {std::to_string(progress.compiled), state});
            }
        }
        m_rowFont->draw(list, ImVec2(at.x + width - m_rowFont->width(state, kText), y), state, kText, toU32(color));
        y += kLine;
    }
    y += 6.0f;
    drawProgressBar(list, ImVec2(at.x, y), width, progress.fraction, m_time);
    y += 30.0f;
    m_rowFont->draw(list, ImVec2(at.x, y), fit(*m_rowFont, progress.activity, kText, width - 90.0f), kText,
                    toU32(colors::kMuted));
    const std::string elapsed = clock(std::chrono::steady_clock::now() - m_started);
    m_rowFont->draw(list, ImVec2(at.x + width - m_rowFont->width(elapsed, kText), y), elapsed, kText,
                    toU32(colors::kRow));
    return y + kLine + 6.0f - at.y;
}

float BuildFlow::drawOutcome(ImDrawList& list, ImVec2 at, float width) const {
    float y = at.y;
    if (m_outcome->ok) {
        mark(list, at, colors::kGood);
        m_rowFont->draw(list, ImVec2(at.x + 18.0f, y), trf("BUILT IN {0}", {clock(m_took)}), kText,
                        toU32(colors::kText));
        y += kLine;
        if (!m_outcome->warning.empty()) {
            y += drawWrapped(list, *m_rowFont, ImVec2(at.x, y), m_outcome->warning, kText, width, toU32(colors::kBad));
        }
        y += drawWrapped(list, *m_rowFont, ImVec2(at.x, y), tr("Next: Play, on the main menu."), kText, width,
                         toU32(colors::kRow));
        return y + 8.0f - at.y;
    }
    mark(list, at, colors::kBad);
    m_rowFont->draw(list, ImVec2(at.x + 18.0f, y),
                    trf("AT: {0}", {tr(kStageNames.at(static_cast<std::size_t>(m_outcome->stage)))}), kText,
                    toU32(colors::kText));
    y += kLine;
    y += drawWrapped(list, *m_rowFont, ImVec2(at.x, y), tr(m_outcome->message), kText, width, toU32(colors::kBad));
    const std::size_t shown = std::min<std::size_t>(5, m_outcome->lastLines.size());
    for (std::size_t i = m_outcome->lastLines.size() - shown; i < m_outcome->lastLines.size(); ++i) {
        m_rowFont->draw(list, ImVec2(at.x, y), fit(*m_rowFont, m_outcome->lastLines[i], 9.0f, width), 9.0f,
                        toU32(colors::kMuted));
        y += 16.0f;
    }
    return y + 8.0f - at.y;
}

Page BuildFlow::page() {
    Page page;
    page.listWidth = 640.0f;
    m_actions.clear();
    const auto add = [&](Action action, MenuRow row) {
        page.rows.push_back(std::move(row));
        m_actions.push_back(action);
    };
    switch (m_screen) {
        case Screen::Checking:
            page.id = "build-checking";
            page.title = tr("BUILD THE GAME");
            page.body = [this](ImDrawList& list, ImVec2 at, float) {
                m_rowFont->draw(list, at, tr("LOOKING FOR THE TOOLS THE BUILD NEEDS..."), kText, toU32(colors::kText));
                return kLine + 8.0f;
            };
            add(Action::Back, {tr("BACK"), "", "", true});
            break;
        case Screen::Tools: {
            page.id = "build-tools";
            page.title = tr("BUILD THE GAME");
            page.body = [this](ImDrawList& list, ImVec2 at, float width) { return drawTools(list, at, width); };
            const bool ready = m_toolchain->ready();
            if (!ready && m_toolchain->canInstall) {
                add(Action::Install,
                    {tr("INSTALL WHAT'S MISSING"), "",
                     tr("Installs them with winget, Windows' package manager. Windows asks you to allow each "
                        "installer. Visual Studio's tools are a large download."),
                     true});
            }
            const bool built = m_project.gameExecutable().has_value();
            add(Action::Build, {built ? tr("BUILD AGAIN") : tr("BUILD"), "",
                                ready ? tr("Turns your disc into the PC game. The first time takes a while.")
                                      : tr("Available once every tool is found."),
                                ready});
            if (ready && core::generatedCodeIsCurrent(m_project)) {
                add(Action::BuildFromScratch,
                    {tr("BUILD FROM SCRATCH"), "",
                     tr("Translates the code from your disc again, even though it is up to date."), true});
            }
            add(Action::CheckAgain,
                {tr("CHECK AGAIN"), "", tr("Looks for the tools again, after you installed some."), true});
            add(Action::Back, {tr("BACK"), "", "", true});
            if (m_notice.empty() && !ready && !m_toolchain->canInstall) {
#ifdef _WIN32
                page.notice =
                    tr("winget (App Installer, from the Microsoft Store) is needed to install the tools. "
                       "Install it, or the missing tools yourself, then check again.");
#else
                page.notice = trf("Install the missing tools with: {0}", {core::installHint(*m_toolchain)});
#endif
                page.noticeIsProblem = true;
            }
            break;
        }
        case Screen::Installing:
            page.id = "build-installing";
            page.title = tr("INSTALLING THE TOOLS");
            page.body = [this](ImDrawList& list, ImVec2 at, float width) { return drawInstalling(list, at, width); };
            add(Action::Stop,
                {m_install.cancelling() ? tr("STOPPING AFTER THIS ONE...") : tr("STOP AFTER THIS ONE"), "",
                 tr("An installer is never cut off halfway; the ones after it are skipped."), !m_install.cancelling()});
            break;
        case Screen::Building:
            page.id = "build-building";
            page.title = tr("BUILDING THE GAME");
            page.body = [this](ImDrawList& list, ImVec2 at, float width) { return drawBuilding(list, at, width); };
            add(Action::Stop,
                {m_build.cancelling() ? tr("STOPPING...") : tr("STOP"), "",
                 tr("Stops the build. Building again carries on from where it stopped."), !m_build.cancelling()});
            break;
        case Screen::Finished:
            page.id = "build-finished";
            page.title = tr("THE GAME IS BUILT");
            page.body = [this](ImDrawList& list, ImVec2 at, float width) { return drawOutcome(list, at, width); };
            add(Action::Continue, {tr("CONTINUE"), "", tr("Back to the main menu, to play."), true});
            add(Action::OpenLog, {tr("OPEN THE LOG"), "", tr("Everything the build printed."), true});
            break;
        case Screen::Failed:
            page.id = "build-failed";
            page.title = m_outcome->stopped ? tr("THE BUILD STOPPED") : tr("THE BUILD FAILED");
            page.body = [this](ImDrawList& list, ImVec2 at, float width) { return drawOutcome(list, at, width); };
            add(Action::TryAgain, {m_outcome->stopped ? tr("CARRY ON BUILDING") : tr("TRY AGAIN"), "",
                                   tr("Builds again, carrying on from what is already done."), true});
            add(Action::OpenLog, {tr("OPEN THE LOG"), "",
                                  tr("Everything the build printed. Attach it if you report the problem."), true});
            add(Action::Back, {tr("BACK"), "", "", true});
            break;
    }
    if (!m_notice.empty()) {
        page.notice = m_notice;
        page.noticeIsProblem = m_noticeIsProblem;
    }
    return page;
}

BuildFlow::Outcome BuildFlow::handle(MenuEvent event) {
    if (event.step != 0 || event.row >= m_actions.size()) {
        return Outcome::Stay;
    }
    switch (m_actions[event.row]) {
        case Action::Install: install(); break;
        case Action::Build: build(false); break;
        case Action::BuildFromScratch: build(true); break;
        case Action::CheckAgain:
            m_notice.clear();
            detect();
            break;
        case Action::Stop:
            if (m_screen == Screen::Installing) {
                m_install.cancel();
            } else {
                m_build.cancel();
            }
            break;
        case Action::TryAgain: build(false); break;
        case Action::OpenLog:
            if (m_outcome) {
                if (auto opened = core::openFolder(m_outcome->log); !opened) {
                    m_notice = opened.error().message;
                    m_noticeIsProblem = true;
                }
            }
            break;
        case Action::Continue:
        case Action::Back: return Outcome::Back;
    }
    return Outcome::Stay;
}

BuildFlow::Outcome BuildFlow::back() {
    if (m_screen == Screen::Installing || m_screen == Screen::Building) {
        return Outcome::Stay;  // Stop is a row of its own
    }
    return Outcome::Back;
}

}  // namespace black::app
