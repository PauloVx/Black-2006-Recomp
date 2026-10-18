#include "app/LauncherLayer.h"

#include "app/BuildFlow.h"
#include "app/EmbeddedAssets.h"
#include "app/KeyCapture.h"
#include "app/NativeDialogs.h"
#include "app/SaveFlow.h"
#include "app/SettingsFlow.h"
#include "app/Theme.h"
#include "app/Translations.h"
#include "core/Files.h"
#include "core/Process.h"
#include "core/Settings.h"
#include "core/Text.h"

#include "Walnut/Application.h"
#include "Walnut/Image.h"

#include <GLFW/glfw3.h>
#include "imgui.h"

#include <cstdlib>
#include <system_error>
#include <utility>

namespace black::app {
namespace {

constexpr float kBannerHeight = 176.0f;
constexpr float kCompactBannerHeight = 120.0f;  // pages with long lists
constexpr float kPageMargin = 64.0f;
constexpr float kMenuWidth = 470.0f;
constexpr float kRowText = 14.0f;
constexpr float kHelpText = 12.0f;

bool isFile(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

std::shared_ptr<Walnut::Image> decodeImage(std::span<const unsigned char> encoded) {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    const std::unique_ptr<void, decltype(&std::free)> pixels(
        Walnut::Image::Decode(encoded.data(), encoded.size(), width, height), &std::free);
    if (!pixels || width == 0 || height == 0) {
        return nullptr;
    }
    return std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA, pixels.get());
}

std::shared_ptr<Walnut::Image> loadImage(const std::filesystem::path& path) {
    if (!isFile(path)) {
        return nullptr;
    }
    const core::Result<std::vector<std::uint8_t>> bytes = core::readFile(path);
    return bytes ? decodeImage(bytes.value()) : nullptr;
}

}  // namespace

LauncherLayer::LauncherLayer(std::filesystem::path start, std::optional<std::filesystem::path> disc)
    : m_start(std::move(start)), m_initialDisc(std::move(disc)) {}

void LauncherLayer::OnAttach() {
    ImGui::GetIO().IniFilename = nullptr;  // no imgui.ini next to the game
    applyTheme();
    acceptDroppedFiles(Walnut::Application::Get().GetWindowHandle());
    installKeyCapture(Walnut::Application::Get().GetWindowHandle());
    refresh();
    if (m_project) {
        setLanguage(core::loadSettings(*m_project).menuLanguage);
    }
    setWindowIcon();
    if (m_initialDisc && m_disc) {
        m_screen = Screen::Disc;
        m_disc->offer(std::move(*m_initialDisc));
    }
}

void LauncherLayer::OnDetach() {
    if (m_session && !m_session->finished()) {
        // The session keeps the game's logs until it ends: wait for it out of sight
        glfwHideWindow(Walnut::Application::Get().GetWindowHandle());
        m_session->wait();
    }
}

void LauncherLayer::refresh() {
    m_project = core::ProjectLayout::locate(m_start);
    m_gameFiles = m_project ? core::inspectGameFiles(m_project->defaultXbe()) : core::GameFilesStatus{};
    m_gameExecutable = m_project ? m_project->gameExecutable() : std::nullopt;
    loadArt();
    if (m_project && !m_disc) {
        m_disc = std::make_unique<DiscFlow>(m_project->gameDirectory(), Walnut::Application::Get().GetWindowHandle());
    }
    if (m_disc) {
        m_disc->setFonts(&m_rowFont, &m_titleFont);
    }
}

void LauncherLayer::loadArt() {
    m_captionFont = MenuFont(Walnut::Application::GetFont(kRowFont));
    if (!m_artFromGame) {
        m_rowFont = MenuFont(Walnut::Application::GetFont(kRowFont));
        m_titleFont = MenuFont(Walnut::Application::GetFont(kTitleFont));
    }
    if (!m_artFromGame && m_project && m_gameFiles.state == core::GameFilesState::Supported) {
        core::GameArt art = core::loadGameArt(*m_project);
        if (art.smallFont) {
            m_rowFont = MenuFont(std::move(*art.smallFont));
        }
        if (art.bigFont) {
            m_titleFont = MenuFont(std::move(*art.bigFont));
        }
        m_artFromGame = m_rowFont.fromGame();
    }
    // The logo is made again only when local/logo.png changed: a texture replaced mid-frame loses the device
    std::optional<std::filesystem::file_time_type> stamp;
    if (m_project) {
        std::error_code error;
        if (const auto written = std::filesystem::last_write_time(m_project->customLogo(), error); !error) {
            stamp = written;
        }
    }
    if (m_logo && stamp == m_logoStamp) {
        return;
    }
    m_logoStamp = stamp;
    m_logo = stamp ? loadImage(m_project->customLogo()) : nullptr;
    if (!m_logo) {
        m_logo = decodeImage(assets::logoImage());
    }
}

void LauncherLayer::setWindowIcon() {
    ImFont* titles = Walnut::Application::GetFont(kTitleFont);
    if (titles == nullptr) {
        return;
    }
    constexpr int kSize = 64;
    std::vector<unsigned char> pixels = makeWindowIcon(*titles, 'B', kSize);
    GLFWimage image{kSize, kSize, pixels.data()};
    glfwSetWindowIcon(Walnut::Application::Get().GetWindowHandle(), 1, &image);
}

bool LauncherLayer::ready() const noexcept {
    return m_gameFiles.state == core::GameFilesState::Supported && m_gameExecutable.has_value();
}

Page LauncherLayer::mainPage() {
    Page page;
    page.id = ready() ? "main" : "setup";
    page.title = ready() ? tr("MAIN MENU") : tr("SETUP");
    page.notice = m_notice;
    page.noticeIsProblem = true;
    m_mainActions.clear();
    const auto add = [&](Action action, MenuRow row) {
        page.rows.push_back(std::move(row));
        m_mainActions.push_back(action);
    };
    const bool canConfigure = m_project.has_value() && !playing();

    MenuRow disc{tr("YOUR DISC"), "", "", m_disc != nullptr && !playing()};
    switch (m_gameFiles.state) {
        case core::GameFilesState::Missing:
            disc.value = tr("CHOOSE");
            disc.help = tr("Choose an ISO of your own original Xbox BLACK disc, or a folder with its files.");
            break;
        case core::GameFilesState::Unreadable:
            disc.value = tr("UNREADABLE");
            disc.help = trf("The game files could not be read: {0}", {m_gameFiles.error});
            break;
        case core::GameFilesState::UnsupportedVersion:
            disc.value = tr("WRONG RELEASE");
            disc.help =
                tr("The game folder holds a different release of BLACK. Choose the original Xbox retail release.");
            break;
        case core::GameFilesState::Supported:
            disc.value = tr("READY");
            disc.help = tr("The supported Xbox retail release of BLACK is in place.");
            break;
    }
    const bool discReady = m_gameFiles.state == core::GameFilesState::Supported;
    MenuRow build{tr("BUILD THE GAME"), m_gameExecutable ? tr("DONE") : tr("NOT BUILT"), "",
                  discReady && m_project.has_value() && !playing()};
    build.help = !discReady         ? tr("Available once your disc is in place.")
                 : m_gameExecutable ? tr("The game is built. Build it again after an update, for example.")
                                    : tr("Turns your disc into the PC game, installing the tools it needs first.");
    if (!ready() && discReady) {
        page.initialRow = 1;  // the next step
    }

    if (playing()) {
        add(Action::Play,
            {tr("STOP GAME"), tr("RUNNING"), tr("Ends the game at once. Save your progress in the game first."), true});
        add(Action::Settings, {tr("SETTINGS"), "", tr("Available when the game is closed."), false});
        add(Action::Saves, {tr("SAVE GAME"), "", tr("Available when the game is closed."), false});
        add(Action::Disc, disc);
        add(Action::Build, build);
    } else if (ready()) {
        add(Action::Play, {tr("PLAY"), "", tr("Starts BLACK with your saved settings."), true});
        add(Action::Settings, {tr("SETTINGS"), "", tr("Display, graphics, controls and language."), canConfigure});
        add(Action::Saves,
            {tr("SAVE GAME"), "", tr("Rename a profile, open missions, or put a backup back."), canConfigure});
        add(Action::Disc, disc);
        add(Action::Build, build);
    } else {
        add(Action::Disc, disc);
        add(Action::Build, build);
        add(Action::Play, {tr("PLAY"), "", tr("Available once your disc is in place and the game is built."), false});
        add(Action::Settings, {tr("SETTINGS"), "", tr("Display, graphics, controls and language."), canConfigure});
        add(Action::Saves,
            {tr("SAVE GAME"), "", tr("Rename a profile, open missions, or put a backup back."), canConfigure});
    }
    add(Action::OpenFolder, {tr("OPEN GAME FOLDER"), "", tr("Opens the folder with the game, your saves and settings."),
                             m_project.has_value()});
    add(Action::Quit, {tr("QUIT"), "", tr("Closes the launcher."), true});
    return page;
}

void LauncherLayer::openDiscPages() {
    if (m_disc) {
        m_disc->restart();
        m_screen = Screen::Disc;
        m_notice.clear();
    }
}

void LauncherLayer::closeFlow() {
    m_screen = Screen::Main;
    m_flow.reset();
    m_refreshPending = true;  // a build or the game's own settings may have changed what the main menu shows
}

void LauncherLayer::openFlow(std::unique_ptr<MenuFlow> flow) {
    m_flow = std::move(flow);
    m_screen = Screen::Flow;
    m_notice.clear();
}

void LauncherLayer::run(Action action) {
    if (!m_project) {
        return;
    }
    core::Result<bool> result = true;
    switch (action) {
        case Action::Play:
            if (playing()) {
                m_session->stop();
            } else {
                startGame();
            }
            return;
        case Action::Settings: openFlow(std::make_unique<SettingsFlow>(*m_project)); break;
        case Action::Saves: openFlow(std::make_unique<SaveFlow>(*m_project)); break;
        case Action::OpenFolder: result = core::openFolder(m_project->root()); break;
        case Action::Quit: Walnut::Application::Get().Close(); break;
        case Action::Disc: openDiscPages(); break;
        case Action::Build: openFlow(std::make_unique<BuildFlow>(*m_project, &m_rowFont)); break;
    }
    m_notice = result ? std::string() : result.error().message;
}

void LauncherLayer::startGame() {
    auto session = core::GameSession::start(*m_project, core::loadSettings(*m_project));
    if (!session) {
        m_notice = session.error().message;
        return;
    }
    m_session = std::move(session).value();
    m_notice.clear();
    m_lastRun = LastRun::Running;
    m_lastLogs = m_session->directory();
}

void LauncherLayer::checkGame() {
    if (!m_session || !m_session->finished()) {
        return;
    }
    const int code = m_session->exitCode().value_or(-1);
    const std::string failure = m_session->logFailure();
    m_lastRun = m_session->stopRequested() ? LastRun::Stopped : code == 0 ? LastRun::Closed : LastRun::Failed;
    m_lastExitCode = code;
    m_notice = failure.empty() ? std::string() : trf("The game's log could not be written: {0}", {failure});
    m_session.reset();
}

void LauncherLayer::OnUIRender() {
    if (std::exchange(m_refreshPending, false)) {
        refresh();  // here, before anything is drawn: it may replace textures
    }
    checkGame();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float titlebar = Walnut::Application::Get().GetTitlebarHeight();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + titlebar));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - titlebar));
    ImGui::Begin("Launcher", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_NoSavedSettings);

    for (std::filesystem::path& dropped : takeDroppedFiles()) {
        if (m_disc && !m_disc->copying() && !playing()) {
            m_screen = Screen::Disc;
            m_disc->offer(std::move(dropped));
        }
    }

    const double time = ImGui::GetTime();
    if (m_screen == Screen::Flow && !m_flow) {
        m_screen = Screen::Main;
    }
    Page page;
    if (m_screen == Screen::Disc && m_disc) {
        page = m_disc->page(time);
    } else if (m_screen == Screen::Flow) {
        m_flow->update(time, m_menu.selected());
        page = m_flow->page();
    } else {
        page = mainPage();
    }
    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const float banner = m_screen == Screen::Main ? kBannerHeight : kCompactBannerHeight;
    drawBanner(list, origin, ImVec2(size.x, banner), {m_logo.get(), &m_captionFont});
    drawPage(list, ImVec2(origin.x, origin.y + banner), ImVec2(size.x, size.y - banner), page);

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        if (m_screen == Screen::Main) {
            if (!playing()) {
                Walnut::Application::Get().Close();
            }
        } else if (m_screen == Screen::Flow) {
            if (m_flow->back() == MenuFlow::Outcome::Back) {
                closeFlow();
            }
        } else if (m_disc && m_disc->back() == DiscFlow::Outcome::Back) {
            m_screen = Screen::Main;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
        m_refreshPending = true;
    }
    ImGui::End();
}

void LauncherLayer::drawPage(ImDrawList& list, ImVec2 topLeft, ImVec2 size, const Page& page) {
    drawRails(list, topLeft.x + 30.0f, topLeft.y + 18.0f, topLeft.y + size.y - 18.0f);
    const bool main = m_screen == Screen::Main;
    const bool settings = m_screen == Screen::Flow;
    const float contentWidth = main ? kMenuWidth : size.x - kPageMargin * 2.0f;
    const float promptY = topLeft.y + size.y - 34.0f;

    float y = topLeft.y + 30.0f;
    m_titleFont.draw(list, ImVec2(topLeft.x + kPageMargin, y), page.title, 22.0f, toU32(colors::kText));
    y += 22.0f + 24.0f;
    if (page.body) {
        y += page.body(list, ImVec2(topLeft.x + kPageMargin, y), contentWidth);
    }

    if (page.id != m_pageId) {
        m_pageId = page.id;
        m_menu.select(page.initialRow);
    }
    MenuList::Layout layout;
    layout.origin = ImVec2(topLeft.x + kPageMargin, y);
    layout.width = main ? kMenuWidth : page.listWidth > 0.0f ? page.listWidth : contentWidth;
    layout.rowHeight = settings ? 28.0f : 31.0f;
    layout.textSize = kRowText;
    layout.selectorEnd = main ? topLeft.x + kPageMargin + kMenuWidth + 32.0f : 0.0f;
    if (page.cellWidth > 0.0f) {
        layout.cellWidth = page.cellWidth;
    }
    constexpr float kHelpSpace = 58.0f;
    layout.visibleRows = static_cast<std::size_t>(std::max(1.0f, (promptY - kHelpSpace - y) / layout.rowHeight));
    const bool acceptInput = !(settings && m_flow->busy());
    if (const auto event = m_menu.draw(page.rows, m_rowFont, layout, acceptInput)) {
        if (settings) {
            if (m_flow->handle(*event) == MenuFlow::Outcome::Back) {
                closeFlow();
            }
        } else if (main && event->step == 0) {
            run(m_mainActions[event->row]);
        } else if (m_disc && event->step == 0) {
            const DiscFlow::Outcome outcome = m_disc->activate(event->row);
            if (outcome != DiscFlow::Outcome::Stay) {
                m_screen = Screen::Main;
            }
            if (outcome == DiscFlow::Outcome::Imported) {
                m_refreshPending = true;
            }
        }
    }
    y += layout.rowHeight * static_cast<float>(m_menu.shownRows()) + 16.0f;

    const bool showNotice = !page.notice.empty();
    const std::string help = showNotice          ? page.notice
                             : page.rows.empty() ? std::string()
                                                 : page.rows[std::min(m_menu.selected(), page.rows.size() - 1)].help;
    drawWrapped(list, m_rowFont, ImVec2(topLeft.x + kPageMargin, y), help, kHelpText, contentWidth,
                showNotice && page.noticeIsProblem ? toU32(colors::kBad) : toU32(colors::kRow));

    if (main) {
        drawStatus(list, ImVec2(topLeft.x + kPageMargin + kMenuWidth + 64.0f, topLeft.y + 30.0f),
                   size.x - (kPageMargin + kMenuWidth + 64.0f) - 40.0f);
    }

    float x = topLeft.x + kPageMargin;
    x += drawPrompt(list, m_rowFont, ImVec2(x, promptY), tr("UP/DOWN"), tr("CHOOSE"), 11.0f) + 26.0f;
    if (settings) {
        x += drawPrompt(list, m_rowFont, ImVec2(x, promptY), tr("LEFT/RIGHT"), tr("CHANGE"), 11.0f) + 26.0f;
    }
    x += drawPrompt(list, m_rowFont, ImVec2(x, promptY), tr("ENTER"), tr("SELECT"), 11.0f);
    const bool canLeave = main ? !playing() : !(m_screen == Screen::Flow && m_flow && !m_flow->canLeave());
    if (canLeave) {  // not while the game runs (the launcher keeps its log) or a build does
        x += 26.0f +
             drawPrompt(list, m_rowFont, ImVec2(x + 26.0f, promptY), tr("ESC"), main ? tr("QUIT") : tr("BACK"), 11.0f);
    }

    const std::string version = trf("LAUNCHER {0}", {BLACK_LAUNCHER_VERSION});
    const float versionX = topLeft.x + size.x - 40.0f - m_rowFont.width(version, 11.0f);
    if (versionX > x + 24.0f) {  // longer languages' prompts take its place
        m_rowFont.draw(list, ImVec2(versionX, promptY), version, 11.0f, toU32(colors::kMuted));
    }
}

void LauncherLayer::drawStatus(ImDrawList& list, ImVec2 topLeft, float width) const {
    const auto line = [&](float y, const std::string& label, const std::string& value, const ImVec4& mark) {
        list.AddCircleFilled(ImVec2(topLeft.x + 4.0f, y + 6.0f), 3.5f, toU32(mark));
        m_rowFont.draw(list, ImVec2(topLeft.x + 16.0f, y), label, 11.0f, toU32(colors::kRow));
        drawWrapped(list, m_rowFont, ImVec2(topLeft.x + 16.0f, y + 19.0f), value, 11.0f, width - 16.0f,
                    toU32(colors::kText));
    };

    m_rowFont.draw(list, topLeft, tr("THIS COPY"), 11.0f, toU32(colors::kMuted));
    list.AddLine(ImVec2(topLeft.x, topLeft.y + 20.0f), ImVec2(topLeft.x + width, topLeft.y + 20.0f),
                 toU32(colors::kRail, 0.25f));
    const bool discReady = m_gameFiles.state == core::GameFilesState::Supported;
    line(topLeft.y + 36.0f, tr("DISC"), discReady ? tr("XBOX RETAIL RELEASE") : tr("NOT IN PLACE"),
         discReady ? colors::kGood : colors::kBad);
    line(topLeft.y + 92.0f, tr("GAME"), m_gameExecutable ? tr("BUILT") : tr("NOT BUILT"),
         m_gameExecutable ? colors::kGood : colors::kBad);
    line(topLeft.y + 148.0f, tr("LOOK"), m_artFromGame ? tr("MENU FONTS FROM YOUR DISC") : tr("STAND-IN FONTS"),
         m_artFromGame ? colors::kGood : colors::kMuted);
    if (m_lastRun != LastRun::None) {
        const std::string state = m_lastRun == LastRun::Running   ? tr("RUNNING")
                                  : m_lastRun == LastRun::Stopped ? tr("STOPPED")
                                  : m_lastRun == LastRun::Closed
                                      ? tr("CLOSED NORMALLY")
                                      : trf("ENDED WITH ERROR {0}", {std::to_string(m_lastExitCode)});
        line(topLeft.y + 204.0f, playing() ? tr("NOW") : tr("LAST GAME"), state,
             m_lastRun == LastRun::Failed ? colors::kBad : colors::kGood);
    }
}

}  // namespace black::app
