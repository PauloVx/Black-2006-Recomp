#pragma once

#include "app/MenuFont.h"
#include "app/Widgets.h"
#include "core/GameArt.h"
#include "core/GameFiles.h"
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

/// The launcher's screen: a banner, then a menu page in the style of BLACK's own (Video Settings and friends),
/// drawn with the game's fonts and logo once the player's disc files are in place.
class LauncherLayer final : public Walnut::Layer {
public:
    /// Fonts Main.cpp adds to the atlas (Walnut::Application::GetFont).
    static constexpr const char* kStencilFont = "Stencil";
    static constexpr const char* kCondensedFont = "Condensed";
    static constexpr const char* kCondensedBoldFont = "CondensedBold";

    explicit LauncherLayer(std::filesystem::path start);

    void OnAttach() override;
    void OnUIRender() override;

private:
    enum class Action { Play, Settings, Disc, Build, OpenFolder, Quit };

    /// Reads the project's state from disk again (after a step changes it, or on request).
    void refresh();
    /// Uses the disc's fonts and logo when they can be read, else the bundled stand-ins.
    void loadArt();
    void setIcons();

    [[nodiscard]] bool ready() const noexcept;
    void buildRows();
    void run(Action action);

    void drawPage(ImDrawList& list, ImVec2 topLeft, ImVec2 size);
    void drawStatus(ImDrawList& list, ImVec2 topLeft, float width) const;

    std::filesystem::path m_start;
    std::optional<core::ProjectLayout> m_project;
    core::GameFilesStatus m_gameFiles;
    std::optional<std::filesystem::path> m_gameExecutable;

    MenuFont m_stencil;
    MenuFont m_rowFont;
    MenuFont m_titleFont;
    MenuFont m_captionFont;
    std::shared_ptr<Walnut::Image> m_logo;
    bool m_artFromGame = false;

    MenuList m_menu;
    std::vector<MenuRow> m_rows;
    std::vector<Action> m_rowActions;
    std::string m_notice;  ///< the result of the last action, when it failed
};

}  // namespace black::app
