#pragma once

#include "app/MenuFlow.h"
#include "app/TextEntry.h"
#include "core/Profile.h"
#include "core/ProjectLayout.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace black::app {

/// The save profile's campaign progress: rename a slot, open or close missions and set their results, or put a
/// backup back. Edits stay pending until saved; every save and restore backs the file up first.
class SaveFlow final : public MenuFlow {
public:
    explicit SaveFlow(core::ProjectLayout project);

    void update(double time, std::size_t selectedRow) override;
    [[nodiscard]] Page page() override;
    Outcome handle(MenuEvent event) override;
    Outcome back() override;
    [[nodiscard]] bool busy() const override { return m_entry.active() || m_inputUsed; }

private:
    enum class Screen { Profile, Slot, Missions, Backups };

    void loadProfiles();
    void load(std::size_t index);
    void change(core::Result<core::profile::Bytes> edited, const std::string& done);
    void saveChanges();
    [[nodiscard]] bool pending() const { return m_working != m_original; }
    [[nodiscard]] std::size_t finished(std::size_t slot) const;
    void say(std::string text, bool problem = false);

    [[nodiscard]] Page profilePage() const;
    [[nodiscard]] Page slotPage() const;
    [[nodiscard]] Page missionsPage() const;
    [[nodiscard]] Page backupsPage() const;

    core::ProjectLayout m_project;
    std::vector<std::filesystem::path> m_profiles;
    std::size_t m_profile = 0;
    core::profile::Bytes m_original;
    core::profile::Bytes m_working;
    std::string m_originalHash;
    std::optional<core::profile::Slots> m_slots;
    std::vector<std::filesystem::path> m_backups;

    Screen m_screen = Screen::Profile;
    std::size_t m_slot = 0;
    std::size_t m_cell = 0;     // missions: 0 open/locked, 1 result
    std::size_t m_slotRow = 0;  // the slot page's selection when it is shown again
    std::size_t m_selectedRow = 0;
    std::optional<std::size_t> m_confirm;  // the row waiting for a second Enter
    TextEntry m_entry;
    bool m_inputUsed = false;
    bool m_leaving = false;  // Escape once more leaves without saving
    double m_time = 0;
    std::string m_notice;
    bool m_noticeIsProblem = false;
};

}  // namespace black::app
