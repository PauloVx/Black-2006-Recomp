#pragma once

#include "app/MenuFlow.h"
#include "app/TextEntry.h"
#include "core/ErrorReport.h"
#include "core/ProjectLayout.h"
#include "core/Settings.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace black::app {

/// The settings pages, in the game's own menu style: each row is a setting that left/right (or a click) changes,
/// saved at once to local/ where the game reads it.
class SettingsFlow final : public MenuFlow {
public:
    explicit SettingsFlow(core::ProjectLayout project);

    /// Reads the settings again (the game's own settings pages may have changed them).
    void reload();
    void update(double time, std::size_t selectedRow) override;
    [[nodiscard]] Page page() override;
    Outcome handle(MenuEvent event) override;
    Outcome back() override;
    [[nodiscard]] bool busy() const override { return m_capture.active || m_entry.active() || m_inputUsed; }

private:
    enum class Section { Hub, Display, Graphics, Controls, Bindings, Gameplay, Game, Diagnostics, Report };

    struct Option {
        std::string label;
        std::string help;
        std::function<std::string()> value;
        std::function<void(int step)> change;  // step -1, +1, or 0 for Enter (usually as +1)
        std::function<bool()> enabled;
        bool adjustable = true;
        /// For typed values: checks and stores the text, or says why it cannot be used.
        std::function<std::optional<std::string>(const std::string& text)> commit{};
        std::string allowed{};  // the characters that can be typed
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
    [[nodiscard]] std::vector<Option> gameplayOptions();
    [[nodiscard]] std::vector<Option> gameOptions();
    [[nodiscard]] std::vector<Option> diagnosticsOptions();
    [[nodiscard]] Page reportPage() const;
    void prepareReport();
    void updateEdit();
    void updateCapture(double time);
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
    TextEntry m_entry;
    std::size_t m_entryRow = 0;
    bool m_inputUsed = false;  // the key or click that ended a capture or edit is not also a menu choice
    double m_time = 0;
    std::optional<std::filesystem::path> m_session;
    std::optional<core::SessionSummary> m_summary;
    std::string m_notice;
    bool m_noticeIsProblem = false;
};

}  // namespace black::app
