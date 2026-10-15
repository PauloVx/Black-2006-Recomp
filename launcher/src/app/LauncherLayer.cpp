#include "app/LauncherLayer.h"

#include "app/EmbeddedAssets.h"
#include "app/Theme.h"
#include "core/Files.h"
#include "core/Process.h"
#include "core/Text.h"

#include "Walnut/Application.h"
#include "Walnut/Image.h"

#include <GLFW/glfw3.h>
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <system_error>
#include <utility>

namespace black::app {
namespace {

constexpr float kBannerHeight = 176.0f;
constexpr float kPageMargin = 64.0f;
constexpr float kMenuWidth = 470.0f;
constexpr float kRowText = 14.0f;
constexpr float kHelpText = 12.0f;

/// Words broken onto lines no wider than `maxWidth`.
std::vector<std::string> wrapText(const MenuFont& font, const std::string& text, float size, float maxWidth) {
    std::vector<std::string> lines;
    std::string line;
    std::size_t start = 0;
    while (start < text.size()) {
        std::size_t end = text.find(' ', start);
        if (end == std::string::npos) {
            end = text.size();
        }
        const std::string word = text.substr(start, end - start);
        const std::string candidate = line.empty() ? word : line + ' ' + word;
        if (!line.empty() && font.width(candidate, size) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = candidate;
        }
        start = end + 1;
    }
    if (!line.empty()) {
        lines.push_back(line);
    }
    return lines;
}

bool isFile(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::is_regular_file(path, error);
}

/// An encoded image (any format stb_image reads) decoded for the GPU, or nothing when it cannot be read.
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

/// An image file decoded for the GPU, or nothing when it is absent or unreadable.
std::shared_ptr<Walnut::Image> loadImage(const std::filesystem::path& path) {
    if (!isFile(path)) {
        return nullptr;
    }
    const core::Result<std::vector<std::uint8_t>> bytes = core::readFile(path);
    return bytes ? decodeImage(bytes.value()) : nullptr;
}

}  // namespace

LauncherLayer::LauncherLayer(std::filesystem::path start) : m_start(std::move(start)) {}

void LauncherLayer::OnAttach() {
    ImGui::GetIO().IniFilename = nullptr;  // nothing to remember between runs: no stray imgui.ini next to the game
    applyTheme();
    refresh();
    setIcons();
}

void LauncherLayer::refresh() {
    m_project = core::ProjectLayout::locate(m_start);
    m_gameFiles = m_project ? core::inspectGameFiles(m_project->defaultXbe()) : core::GameFilesStatus{};
    m_gameExecutable = m_project ? m_project->gameExecutable() : std::nullopt;
    loadArt();
    buildRows();
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
    if (m_project) {
        m_logo = loadImage(m_project->customLogo());
    }
    if (!m_logo) {
        m_logo = decodeImage(assets::logoImage());
    }
}

void LauncherLayer::setIcons() {
    ImFont* bold = Walnut::Application::GetFont(kTitleFont);
    if (bold == nullptr) {
        return;
    }
    // The taskbar icon: a tan "B" on a dark square, readable on any theme (the titlebar itself shows no icon).
    IconPixels window = makeGlyphIcon(*bold, 'B', 64, true);
    GLFWimage image{window.size, window.size, window.rgba.data()};
    glfwSetWindowIcon(Walnut::Application::Get().GetWindowHandle(), 1, &image);
}

bool LauncherLayer::ready() const noexcept {
    return m_gameFiles.state == core::GameFilesState::Supported && m_gameExecutable.has_value();
}

void LauncherLayer::buildRows() {
    m_rows.clear();
    m_rowActions.clear();
    const auto add = [this](Action action, MenuRow row) {
        m_rows.push_back(std::move(row));
        m_rowActions.push_back(action);
    };
    const bool canPlay = ready() && m_project.has_value() && isFile(m_project->legacyPlay());
    const bool canConfigure = m_project.has_value() && isFile(m_project->legacySettings());

    MenuRow disc{"YOUR DISC", "", "", false};
    switch (m_gameFiles.state) {
        case core::GameFilesState::Missing:
            disc.value = "NOT FOUND";
            disc.help =
                "The game needs the files of your own original Xbox BLACK disc. Choosing an ISO here comes "
                "with the setup wizard; for now, extract the disc into the game folder.";
            break;
        case core::GameFilesState::Unreadable:
            disc.value = "UNREADABLE";
            disc.help = "The game files could not be read: " + m_gameFiles.error;
            break;
        case core::GameFilesState::UnsupportedVersion:
            disc.value = "WRONG RELEASE";
            disc.help =
                "These files are from a different release of BLACK. Only the original Xbox retail release "
                "is supported.";
            break;
        case core::GameFilesState::Supported:
            disc.value = "READY";
            disc.help = "The supported Xbox retail release of BLACK is in place.";
            break;
    }
    MenuRow build{"BUILD THE GAME", m_gameExecutable ? "DONE" : "NOT BUILT", "", false};
    build.help = m_gameExecutable ? "The PC game has been built from your disc."
                                  : "Turns your disc into the PC game. Building from the launcher comes with the setup "
                                    "wizard; for now, run Build BLACK.";

    if (ready()) {
        add(Action::Play, {"PLAY", "", "Starts BLACK with your saved settings.", canPlay});
        add(Action::Settings, {"SETTINGS", "", "Display, graphics, controls and language.", canConfigure});
        add(Action::Disc, disc);
        add(Action::Build, build);
    } else {
        add(Action::Disc, disc);
        add(Action::Build, build);
        add(Action::Play, {"PLAY", "", "Available once your disc is in place and the game is built.", false});
        add(Action::Settings, {"SETTINGS", "", "Display, graphics, controls and language.", canConfigure});
    }
    add(Action::OpenFolder,
        {"OPEN GAME FOLDER", "", "Opens the folder with the game, your saves and settings.", m_project.has_value()});
    add(Action::Quit, {"QUIT", "", "Closes the launcher.", true});
}

void LauncherLayer::run(Action action) {
    if (!m_project) {
        return;
    }
    core::Result<bool> result = true;
    switch (action) {
        case Action::Play: result = core::startDetached(m_project->legacyPlay(), {}, m_project->root()); break;
        case Action::Settings: result = core::startDetached(m_project->legacySettings(), {}, m_project->root()); break;
        case Action::OpenFolder: result = core::openFolder(m_project->root()); break;
        case Action::Quit: Walnut::Application::Get().Close(); break;
        case Action::Disc:
        case Action::Build: break;
    }
    m_notice = result ? std::string() : result.error().message;
}

void LauncherLayer::OnUIRender() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float titlebar = Walnut::Application::Get().GetTitlebarHeight();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + titlebar));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - titlebar));
    ImGui::Begin("Launcher", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
                     ImGuiWindowFlags_NoSavedSettings);

    ImDrawList& list = *ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    drawBanner(list, origin, ImVec2(size.x, kBannerHeight), {m_logo.get(), &m_captionFont});
    drawPage(list, ImVec2(origin.x, origin.y + kBannerHeight), ImVec2(size.x, size.y - kBannerHeight));

    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        Walnut::Application::Get().Close();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) {
        refresh();
    }
    ImGui::End();
}

void LauncherLayer::drawPage(ImDrawList& list, ImVec2 topLeft, ImVec2 size) {
    drawRails(list, topLeft.x + 30.0f, topLeft.y + 18.0f, topLeft.y + size.y - 18.0f);

    // The page title, as on the game's own pages (the logo is in the banner above)
    float y = topLeft.y + 30.0f;
    m_titleFont.draw(list, ImVec2(topLeft.x + kPageMargin, y), ready() ? "MAIN MENU" : "SETUP", 22.0f,
                     toU32(colors::kText));
    y += 22.0f + 24.0f;

    // The menu
    MenuList::Layout layout;
    layout.origin = ImVec2(topLeft.x + kPageMargin, y);
    layout.width = kMenuWidth;
    layout.rowHeight = 31.0f;
    layout.textSize = kRowText;
    layout.selectorEnd = topLeft.x + kPageMargin + kMenuWidth + 32.0f;
    if (const auto activated = m_menu.draw(m_rows, m_rowFont, layout)) {
        run(m_rowActions[*activated]);
    }
    y += layout.rowHeight * static_cast<float>(m_rows.size()) + 16.0f;

    // Help for the selected row (or what went wrong), under the list like the game's
    const std::string& help = !m_notice.empty() ? m_notice : m_rows[m_menu.selected()].help;
    const ImU32 helpColor = !m_notice.empty() ? toU32(colors::kBad) : toU32(colors::kRow);
    for (const std::string& line : wrapText(m_rowFont, help, kHelpText, kMenuWidth)) {
        m_rowFont.draw(list, ImVec2(topLeft.x + kPageMargin, y), line, kHelpText, helpColor);
        y += kHelpText + 9.0f;
    }

    drawStatus(list, ImVec2(topLeft.x + kPageMargin + kMenuWidth + 64.0f, topLeft.y + 30.0f),
               size.x - (kPageMargin + kMenuWidth + 64.0f) - 40.0f);

    // Prompts along the bottom, like the game's button prompts
    const float promptY = topLeft.y + size.y - 34.0f;
    float x = topLeft.x + kPageMargin;
    x += drawPrompt(list, m_rowFont, ImVec2(x, promptY), "UP/DOWN", "CHOOSE", 11.0f) + 26.0f;
    x += drawPrompt(list, m_rowFont, ImVec2(x, promptY), "ENTER", "SELECT", 11.0f) + 26.0f;
    drawPrompt(list, m_rowFont, ImVec2(x, promptY), "ESC", "QUIT", 11.0f);

    const std::string version = std::string("LAUNCHER ") + BLACK_LAUNCHER_VERSION;
    m_rowFont.draw(list, ImVec2(topLeft.x + size.x - 40.0f - m_rowFont.width(version, 11.0f), promptY), version, 11.0f,
                   toU32(colors::kMuted));
}

void LauncherLayer::drawStatus(ImDrawList& list, ImVec2 topLeft, float width) const {
    const auto line = [&](float y, const char* label, const std::string& value, const ImVec4& mark) {
        list.AddCircleFilled(ImVec2(topLeft.x + 4.0f, y + 6.0f), 3.5f, toU32(mark));
        m_rowFont.draw(list, ImVec2(topLeft.x + 16.0f, y), label, 11.0f, toU32(colors::kRow));
        for (const std::string& text : wrapText(m_rowFont, value, 11.0f, width - 16.0f)) {
            y += 19.0f;
            m_rowFont.draw(list, ImVec2(topLeft.x + 16.0f, y), text, 11.0f, toU32(colors::kText));
        }
    };

    m_rowFont.draw(list, topLeft, "THIS COPY", 11.0f, toU32(colors::kMuted));
    list.AddLine(ImVec2(topLeft.x, topLeft.y + 20.0f), ImVec2(topLeft.x + width, topLeft.y + 20.0f),
                 toU32(colors::kRail, 0.25f));
    const bool discReady = m_gameFiles.state == core::GameFilesState::Supported;
    line(topLeft.y + 36.0f, "DISC", discReady ? "XBOX RETAIL RELEASE" : "NOT IN PLACE",
         discReady ? colors::kGood : colors::kBad);
    line(topLeft.y + 92.0f, "GAME", m_gameExecutable ? "BUILT" : "NOT BUILT",
         m_gameExecutable ? colors::kGood : colors::kBad);
    line(topLeft.y + 148.0f, "LOOK", m_artFromGame ? "MENU FONTS FROM YOUR DISC" : "STAND-IN FONTS",
         m_artFromGame ? colors::kGood : colors::kMuted);
}

}  // namespace black::app
