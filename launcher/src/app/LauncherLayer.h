#pragma once

#include "app/DiscFlow.h"
#include "app/MenuFont.h"
#include "app/Widgets.h"
#include "core/GameFiles.h"
#include "core/GameSession.h"
#include "core/ProjectLayout.h"

#include "Walnut/Layer.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Walnut {
class Image;
}

namespace black::app {

class LauncherLayer final : public Walnut::Layer {
public:
    static constexpr const char* kRowFont = "Rows";
    static constexpr const char* kTitleFont = "Titles";

    /// `disc`: an ISO or folder to open with, from the command line (or dropped on the icon)
    LauncherLayer(std::filesystem::path start, std::optional<std::filesystem::path> disc);

    void OnAttach() override;
    void OnDetach() override;
    void OnUIRender() override;

private:
    enum class Screen { Main, Disc };
    enum class Action { Play, Settings, Disc, Build, OpenFolder, Quit };

    void refresh();
    void loadArt();
    void setWindowIcon();

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] Page mainPage();
    void run(Action action);
    void openDiscPages();
    void startGame();
    void checkGame();
    [[nodiscard]] bool playing() const noexcept { return m_session != nullptr; }

    void drawPage(ImDrawList& list, ImVec2 topLeft, ImVec2 size, const Page& page);
    void drawStatus(ImDrawList& list, ImVec2 topLeft, float width) const;

    std::filesystem::path m_start;
    std::optional<std::filesystem::path> m_initialDisc;
    std::optional<core::ProjectLayout> m_project;
    core::GameFilesStatus m_gameFiles;
    std::optional<std::filesystem::path> m_gameExecutable;

    MenuFont m_rowFont;
    MenuFont m_titleFont;
    MenuFont m_captionFont;
    std::shared_ptr<Walnut::Image> m_logo;
    bool m_artFromGame = false;

    Screen m_screen = Screen::Main;
    std::unique_ptr<DiscFlow> m_disc;
    MenuList m_menu;
    std::string m_pageId;
    std::vector<Action> m_mainActions;
    std::string m_notice;

    std::unique_ptr<core::GameSession> m_session;
    std::string m_lastRun;
    std::filesystem::path m_lastLogs;
};

}  // namespace black::app
