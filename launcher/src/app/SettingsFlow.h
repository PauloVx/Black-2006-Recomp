#pragma once

#include "app/Widgets.h"
#include "core/ProjectLayout.h"
#include "core/Settings.h"

#include <functional>
#include <string>
#include <vector>

namespace black::app {

/// The settings pages, in the game's own menu style: each row is a setting that left/right (or a click) changes,
/// saved at once to local/ where the game reads it.
class SettingsFlow {
public:
    enum class Outcome { Stay, Back };

    explicit SettingsFlow(core::ProjectLayout project);

    /// Reads the settings again (the game's own settings pages may have changed them).
    void reload();
    /// Call each frame before page(): finishes a key capture. `selectedRow` is the menu's selection.
    void update(double time, std::size_t selectedRow);
    [[nodiscard]] Page page();
    Outcome handle(MenuEvent event);
    Outcome back();
    [[nodiscard]] bool capturing() const noexcept { return m_capture.active; }

private:
    enum class Section { Hub, Display, Graphics, Controls, Bindings, Game };

    struct Option {
        std::string label;
        std::string help;
        std::function<std::string()> value;
        std::function<void(int step)> change;  // step -1, +1, or 0 for Enter (usually as +1)
        std::function<bool()> enabled;
        bool adjustable = true;
    };

    struct Capture {
        bool active = false;
        std::size_t action = 0;
        double since = 0;
    };

    [[nodiscard]] std::vector<Option> options(Section section);
    [[nodiscard]] std::vector<Option> displayOptions();
    [[nodiscard]] std::vector<Option> graphicsOptions();
    [[nodiscard]] std::vector<Option> controlsOptions();
    [[nodiscard]] std::vector<Option> gameOptions();
    [[nodiscard]] Page bindingsPage() const;
    [[nodiscard]] std::vector<std::string> slotsOf(std::size_t action) const;
    void setSlot(std::size_t action, std::size_t slot, const std::string& token);
    void open(Section section);
    void save();

    core::ProjectLayout m_project;
    core::Settings m_settings;
    Section m_section = Section::Hub;
    Section m_lastSection = Section::Display;
    std::size_t m_slot = 0;
    std::size_t m_selectedRow = 0;
    Capture m_capture;
    std::string m_notice;
    bool m_noticeIsProblem = false;
};

}  // namespace black::app
