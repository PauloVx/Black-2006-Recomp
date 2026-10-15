#include "app/SaveFlow.h"

#include "core/Files.h"
#include "core/Process.h"
#include "core/Text.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <utility>

namespace black::app {
namespace {

namespace profile = core::profile;

constexpr std::array<const char*, profile::kDifficulties> kDifficulties = {"EASY", "NORMAL", "HARD", "BLACK OPS"};

std::string resultName(std::uint32_t result) {
    switch (result) {
        case 0: return "DONE";
        case 1: return "BELOW TARGET";
        case 2: return "TARGET MET";
        default: return "-";
    }
}

std::string upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

}  // namespace

SaveFlow::SaveFlow(core::ProjectLayout project) : m_project(std::move(project)) { loadProfiles(); }

void SaveFlow::say(std::string text, bool problem) {
    m_notice = std::move(text);
    m_noticeIsProblem = problem;
}

void SaveFlow::loadProfiles() {
    m_profiles = profile::findProfiles(m_project);
    m_profile = std::min(m_profile, m_profiles.empty() ? 0 : m_profiles.size() - 1);
    if (m_profiles.empty()) {
        m_slots.reset();
        return;
    }
    load(m_profile);
}

void SaveFlow::load(std::size_t index) {
    m_profile = index;
    m_slots.reset();
    const auto bytes = core::readFile(m_profiles[index]);
    if (!bytes) {
        say(bytes.error().message, true);
        return;
    }
    m_original.assign(bytes.value().begin(), bytes.value().end());
    m_working = m_original;
    m_originalHash = profile::sha256Of(m_original);
    auto slots = profile::read(m_original);
    if (!slots) {
        say(slots.error().message, true);
        return;
    }
    m_slots = std::move(slots).value();
    m_backups = profile::listBackups(profile::backupFolder(m_project, m_profiles[index]));
}

std::size_t SaveFlow::finished(std::size_t slot) const {
    const auto& results = m_slots->at(slot).results;
    return static_cast<std::size_t>(
        std::count_if(results.begin(), results.end(), [](std::uint32_t r) { return r != profile::kNoResult; }));
}

void SaveFlow::change(core::Result<profile::Bytes> edited, const std::string& done) {
    if (!edited) {
        say(edited.error().message, true);
        return;
    }
    m_working = std::move(edited).value();
    m_slots = profile::read(m_working).value();
    say(done + " Not saved yet: choose Save changes.");
}

void SaveFlow::saveChanges() {
    const auto& path = m_profiles[m_profile];
    const auto applied =
        profile::apply(path, m_working, profile::backupFolder(m_project, path), m_originalHash);
    if (!applied) {
        say(applied.error().message, true);
        return;
    }
    const std::string after = profile::sha256Of(m_working);
    if (applied.value()) {
        profile::logAction(m_project, "APPLIED", m_slot, path, m_originalHash, after, core::toUtf8(*applied.value()));
    }
    load(m_profile);
    say(applied.value() ? "Saved. The file as it was is kept in the backups." : "Nothing had changed.");
}

void SaveFlow::update(double time, std::size_t selectedRow) {
    m_time = time;
    if (selectedRow != m_selectedRow) {
        m_confirm.reset();
    }
    m_selectedRow = selectedRow;
    const bool wasBusy = m_entry.active();
    m_inputUsed = false;
    if (m_entry.update()) {
        m_entry.end();
        change(profile::setName(m_working, m_slot, m_entry.text()), "Renamed.");
        if (m_noticeIsProblem) {
            m_entry.begin(m_entry.text(), {}, profile::kMaxNameLength);
        }
    }
    m_inputUsed = wasBusy && !m_entry.active();
    if (m_screen != Screen::Missions || m_entry.active() || selectedRow >= profile::kMissions) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        m_cell = 1 - m_cell;
    } else if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
        change(profile::restoreMission(m_working, m_original, m_slot, selectedRow), "Mission put back as it was.");
    }
}

Page SaveFlow::profilePage() const {
    Page page;
    page.id = "saves";
    page.title = "SAVE GAME";
    page.listWidth = 640.0f;
    if (!m_slots) {
        page.rows = {{"NO SAVE YET", "", "Play and make a profile in the game first; it appears here.", false},
                     {"BACK", "", "", true}};
        return page;
    }
    if (m_profiles.size() > 1) {
        page.rows.push_back({"PROFILE FILE", core::toUtf8(m_profiles[m_profile].parent_path().filename()),
                             "This game folder has more than one save; left and right choose which.", true, true});
    }
    for (std::size_t slot = 0; slot < profile::kSlots; ++slot) {
        const profile::Slot& data = m_slots->at(slot);
        MenuRow row{"SLOT " + std::to_string(slot + 1), "EMPTY", "An empty slot. Make a profile in it in the game.",
                    data.active};
        if (data.active) {
            row.value = upper(data.name) + "   " + std::to_string(finished(slot)) + " OF 32 DONE";
            row.help = "Rename this profile, or change its missions.";
        }
        page.rows.push_back(std::move(row));
    }
    page.rows.push_back({"BACKUPS", std::to_string(m_backups.size()),
                         "Every save and restore keeps the file as it was. Put one of them back.", !m_backups.empty()});
    page.rows.push_back({"OPEN SAVE FOLDER", "", "The folder with the profile file.", true});
    page.rows.push_back({"BACK", "", "", true});
    return page;
}

Page SaveFlow::slotPage() const {
    const profile::Slot& data = m_slots->at(m_slot);
    Page page;
    page.id = "saves-slot";
    page.title = "SLOT " + std::to_string(m_slot + 1) + ":  " + upper(data.name);
    page.listWidth = 640.0f;
    MenuRow name{"NAME", upper(data.name), "Enter to type a new name (letters, digits and punctuation).", true};
    if (m_entry.active()) {
        name.value = m_entry.shown(m_time);
        name.help = "Type the name, then Enter. Escape keeps the old one.";
    }
    page.rows.push_back(std::move(name));
    page.rows.push_back({"MISSIONS", std::to_string(finished(m_slot)) + " OF 32 DONE",
                         "Which missions are open on each difficulty, and their results.", true});
    page.rows.push_back(
        {"UNLOCK EVERY MISSION", "", "Opens every mission on every difficulty, with its target met.", true});
    page.rows.push_back({"SAVE CHANGES", pending() ? "PENDING" : "",
                         "Writes the changes to the profile, keeping the old file in the backups.", pending()});
    page.rows.push_back({"UNDO CHANGES", "", "Forgets the changes not saved yet.", pending()});
    page.rows.push_back({"BACK", "", "", true});
    page.initialRow = m_slotRow;
    return page;
}

Page SaveFlow::missionsPage() const {
    const profile::Slot& data = m_slots->at(m_slot);
    Page page;
    page.id = "saves-missions";
    page.title = "MISSIONS:  " + upper(data.name);
    page.listWidth = 640.0f;
    for (std::size_t mission = 0; mission < profile::kMissions; ++mission) {
        MenuRow row{std::string(kDifficulties.at(mission / profile::kMissionsPerDifficulty)) + "  " +
                        std::to_string(mission % profile::kMissionsPerDifficulty + 1),
                    "",
                    "Enter changes the framed value; left and right choose open or result; Delete puts the mission "
                    "back as it was.",
                    true};
        row.cells = {data.available.at(mission) ? "OPEN" : "LOCKED", resultName(data.results.at(mission))};
        if (mission == m_selectedRow) {
            row.markedCell = static_cast<int>(m_cell);
        }
        page.rows.push_back(std::move(row));
    }
    page.rows.push_back({"BACK", "", "", true});
    return page;
}

Page SaveFlow::backupsPage() const {
    Page page;
    page.id = "saves-backups";
    page.title = "BACKUPS";
    page.listWidth = 640.0f;
    for (std::size_t i = 0; i < m_backups.size(); ++i) {
        const bool beforeRestore = core::toUtf8(m_backups[i].filename()).starts_with("Profile_before_restore_");
        page.rows.push_back({profile::backupTime(m_backups[i]).value_or(core::toUtf8(m_backups[i].filename())), beforeRestore ? "BEFORE A RESTORE" : "BEFORE A SAVE",
                             "Puts this file back. The profile as it is now is backed up first.",
                             true});
    }
    page.rows.push_back({"BACK", "", "", true});
    return page;
}

Page SaveFlow::page() {
    Page page;
    if (!m_slots || m_screen == Screen::Profile) {
        page = profilePage();
    } else if (m_screen == Screen::Slot) {
        page = slotPage();
    } else if (m_screen == Screen::Missions) {
        page = missionsPage();
        page.cellWidth = 150.0f;
    } else {
        page = backupsPage();
    }
    if (!m_notice.empty()) {
        page.notice = m_notice;
        page.noticeIsProblem = m_noticeIsProblem;
    } else if (pending() && m_screen != Screen::Backups) {
        page.notice = m_screen == Screen::Slot ? "Not saved yet: choose Save changes."
                                               : "Not saved yet: Save changes on the slot's page writes them.";
    }
    return page;
}

SaveFlow::Outcome SaveFlow::handle(MenuEvent event) {
    if (m_entry.active()) {
        return Outcome::Stay;
    }
    const std::optional<std::size_t> confirmed = std::exchange(m_confirm, std::nullopt);
    m_notice.clear();
    if (!m_slots || m_screen == Screen::Profile) {
        if (!m_slots) {
            return Outcome::Back;
        }
        std::size_t row = event.row;
        if (m_profiles.size() > 1) {
            if (row == 0) {
                if (pending()) {
                    say("Save or undo this profile's changes first.", true);
                } else {
                    const auto count = static_cast<long long>(m_profiles.size());
                    const long long step = event.step == 0 ? 1 : event.step;
                    load(static_cast<std::size_t>((static_cast<long long>(m_profile) + step + count) % count));
                }
                return Outcome::Stay;
            }
            --row;
        }
        if (row < profile::kSlots) {
            m_slot = row;
            m_slotRow = 0;
            m_screen = Screen::Slot;
        } else if (row == profile::kSlots) {
            m_screen = Screen::Backups;
        } else if (row == profile::kSlots + 1) {
            if (auto opened = core::openFolder(m_profiles[m_profile].parent_path()); !opened) {
                say(opened.error().message, true);
            }
        } else {
            return back();
        }
        return Outcome::Stay;
    }
    if (m_screen == Screen::Slot) {
        switch (event.row) {
            case 0: m_entry.begin(m_slots->at(m_slot).name, {}, profile::kMaxNameLength); break;
            case 1:
                m_screen = Screen::Missions;
                m_cell = 0;
                m_slotRow = 1;
                break;
            case 2:
                if (confirmed == 2u) {
                    change(profile::unlockAll(m_working, m_slot), "Every mission is open with its target met.");
                } else {
                    m_confirm = 2;
                    say("Press Enter again to open every mission on every difficulty, with its target met.");
                }
                break;
            case 3: saveChanges(); break;
            case 4:
                m_working = m_original;
                m_slots = profile::read(m_working).value();
                say("Changes undone.");
                break;
            default: return back();
        }
        return Outcome::Stay;
    }
    if (m_screen == Screen::Missions) {
        if (event.row >= profile::kMissions) {
            return back();
        }
        const profile::Slot& data = m_slots->at(m_slot);
        if (m_cell == 0) {
            change(profile::setAvailable(m_working, m_slot, event.row, !data.available.at(event.row)),
                   "Mission changed.");
        } else {
            const std::uint32_t current = data.results.at(event.row);
            const std::uint32_t next = current == profile::kNoResult ? 0
                                       : current >= profile::kHighestResult ? profile::kNoResult
                                                                            : current + 1;
            change(profile::setResult(m_working, m_slot, event.row, next), "Mission changed.");
        }
        return Outcome::Stay;
    }
    if (event.row >= m_backups.size()) {
        return back();
    }
    if (confirmed != event.row) {
        m_confirm = event.row;
        say("Press Enter again to put this backup back. The profile as it is now is backed up first.");
        return Outcome::Stay;
    }
    const auto& path = m_profiles[m_profile];
    const auto restored = profile::restore(path, m_backups[event.row], profile::backupFolder(m_project, path));
    if (!restored) {
        say(restored.error().message, true);
        return Outcome::Stay;
    }
    profile::logAction(m_project, "RESTORED", m_slot, path, m_originalHash, "", core::toUtf8(m_backups[event.row]));
    load(m_profile);
    m_screen = Screen::Profile;
    say("Backup put back. The file it replaced is in the backups too.");
    return Outcome::Stay;
}

SaveFlow::Outcome SaveFlow::back() {
    m_confirm.reset();
    if (m_entry.active()) {
        m_entry.end();
        m_notice.clear();
        return Outcome::Stay;
    }
    switch (m_screen) {
        case Screen::Missions: m_screen = Screen::Slot; return Outcome::Stay;
        case Screen::Slot:
        case Screen::Backups: m_screen = Screen::Profile; return Outcome::Stay;
        case Screen::Profile: break;
    }
    if (pending()) {
        if (m_notice.starts_with("Unsaved")) {
            return Outcome::Back;  // the second Escape leaves them
        }
        say("Unsaved changes. Escape again to leave without them, or open the slot and save.", true);
        return Outcome::Stay;
    }
    return Outcome::Back;
}

}  // namespace black::app
