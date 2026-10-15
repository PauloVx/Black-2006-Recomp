#include "app/SaveFlow.h"

#include "app/Translations.h"
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

constexpr std::array<const char*, profile::kDifficulties> kDifficulties = {trMark("EASY"), trMark("NORMAL"),
                                                                           trMark("HARD"), trMark("BLACK OPS")};

std::string resultName(std::uint32_t result) {
    switch (result) {
        case 0: return tr("DONE");
        case 1: return tr("BELOW TARGET");
        case 2: return tr("TARGET MET");
        default: return "-";
    }
}

std::string upper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

}  // namespace

SaveFlow::SaveFlow(core::ProjectLayout project) : m_project(std::move(project)) {
    loadProfiles();
}

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
    const auto applied = profile::apply(path, m_working, profile::backupFolder(m_project, path), m_originalHash);
    if (!applied) {
        say(applied.error().message, true);
        return;
    }
    const std::string after = profile::sha256Of(m_working);
    if (applied.value()) {
        profile::logAction(m_project, "APPLIED", m_slot, path, m_originalHash, after, core::toUtf8(*applied.value()));
    }
    load(m_profile);
    say(applied.value() ? tr("Saved. The file as it was is kept in the backups.") : tr("Nothing had changed."));
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
        change(profile::setName(m_working, m_slot, m_entry.text()), tr("Renamed."));
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
        change(profile::restoreMission(m_working, m_original, m_slot, selectedRow), tr("Mission put back as it was."));
    }
}

Page SaveFlow::profilePage() const {
    Page page;
    page.id = "saves";
    page.title = tr("SAVE GAME");
    page.listWidth = 640.0f;
    if (!m_slots) {
        page.rows = {{tr("NO SAVE YET"), "", tr("Play and make a profile in the game first; it appears here."), false},
                     {tr("BACK"), "", "", true}};
        return page;
    }
    if (m_profiles.size() > 1) {
        page.rows.push_back({tr("PROFILE FILE"), core::toUtf8(m_profiles[m_profile].parent_path().filename()),
                             tr("This game folder has more than one save; left and right choose which."), true, true});
    }
    for (std::size_t slot = 0; slot < profile::kSlots; ++slot) {
        const profile::Slot& data = m_slots->at(slot);
        MenuRow row{trf("SLOT {0}", {std::to_string(slot + 1)}), tr("EMPTY"),
                    tr("An empty slot. Make a profile in it in the game."), data.active};
        if (data.active) {
            row.value = upper(data.name) + "   " + trf("{0} OF 32 DONE", {std::to_string(finished(slot))});
            row.help = tr("Rename this profile, or change its missions.");
        }
        page.rows.push_back(std::move(row));
    }
    page.rows.push_back({tr("BACKUPS"), std::to_string(m_backups.size()),
                         tr("Every save and restore keeps the file as it was. Put one of them back."),
                         !m_backups.empty()});
    page.rows.push_back({tr("OPEN SAVE FOLDER"), "", tr("The folder with the profile file."), true});
    page.rows.push_back({tr("BACK"), "", "", true});
    return page;
}

Page SaveFlow::slotPage() const {
    const profile::Slot& data = m_slots->at(m_slot);
    Page page;
    page.id = "saves-slot";
    page.title = trf("SLOT {0}:  {1}", {std::to_string(m_slot + 1), upper(data.name)});
    page.listWidth = 640.0f;
    MenuRow name{tr("NAME"), upper(data.name), tr("Enter to type a new name (letters, digits and punctuation)."), true};
    if (m_entry.active()) {
        name.value = m_entry.shown(m_time);
        name.help = tr("Type the name, then Enter. Escape keeps the old one.");
    }
    page.rows.push_back(std::move(name));
    page.rows.push_back({tr("MISSIONS"), trf("{0} OF 32 DONE", {std::to_string(finished(m_slot))}),
                         tr("Which missions are open on each difficulty, and their results."), true});
    page.rows.push_back(
        {tr("UNLOCK EVERY MISSION"), "", tr("Opens every mission on every difficulty, with its target met."), true});
    page.rows.push_back({tr("SAVE CHANGES"), pending() ? tr("PENDING") : "",
                         tr("Writes the changes to the profile, keeping the old file in the backups."), pending()});
    page.rows.push_back({tr("UNDO CHANGES"), "", tr("Forgets the changes not saved yet."), pending()});
    page.rows.push_back({tr("BACK"), "", "", true});
    page.initialRow = m_slotRow;
    return page;
}

Page SaveFlow::missionsPage() const {
    const profile::Slot& data = m_slots->at(m_slot);
    Page page;
    page.id = "saves-missions";
    page.title = trf("MISSIONS:  {0}", {upper(data.name)});
    page.listWidth = 640.0f;
    for (std::size_t mission = 0; mission < profile::kMissions; ++mission) {
        MenuRow row{tr(kDifficulties.at(mission / profile::kMissionsPerDifficulty)) + "  " +
                        std::to_string(mission % profile::kMissionsPerDifficulty + 1),
                    "",
                    tr("Enter changes the framed value; left and right choose open or result; Delete puts the mission "
                       "back as it was."),
                    true};
        row.cells = {data.available.at(mission) ? tr("OPEN") : tr("LOCKED"), resultName(data.results.at(mission))};
        if (mission == m_selectedRow) {
            row.markedCell = static_cast<int>(m_cell);
        }
        page.rows.push_back(std::move(row));
    }
    page.rows.push_back({tr("BACK"), "", "", true});
    return page;
}

Page SaveFlow::backupsPage() const {
    Page page;
    page.id = "saves-backups";
    page.title = tr("BACKUPS");
    page.listWidth = 640.0f;
    for (std::size_t i = 0; i < m_backups.size(); ++i) {
        const bool beforeRestore = core::toUtf8(m_backups[i].filename()).starts_with("Profile_before_restore_");
        page.rows.push_back({profile::backupTime(m_backups[i]).value_or(core::toUtf8(m_backups[i].filename())),
                             beforeRestore ? tr("BEFORE A RESTORE") : tr("BEFORE A SAVE"),
                             tr("Puts this file back. The profile as it is now is backed up first."), true});
    }
    page.rows.push_back({tr("BACK"), "", "", true});
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
        page.notice = m_screen == Screen::Slot ? tr("Not saved yet: choose Save changes.")
                                               : tr("Not saved yet: Save changes on the slot's page writes them.");
    }
    return page;
}

SaveFlow::Outcome SaveFlow::handle(MenuEvent event) {
    if (m_entry.active()) {
        return Outcome::Stay;
    }
    const std::optional<std::size_t> confirmed = std::exchange(m_confirm, std::nullopt);
    m_leaving = false;
    m_notice.clear();
    if (!m_slots || m_screen == Screen::Profile) {
        if (!m_slots) {
            return Outcome::Back;
        }
        std::size_t row = event.row;
        if (m_profiles.size() > 1) {
            if (row == 0) {
                if (pending()) {
                    say(tr("Save or undo this profile's changes first."), true);
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
                    change(profile::unlockAll(m_working, m_slot), tr("Every mission is open with its target met."));
                } else {
                    m_confirm = 2;
                    say(tr("Press Enter again to open every mission on every difficulty, with its target met."));
                }
                break;
            case 3: saveChanges(); break;
            case 4:
                m_working = m_original;
                m_slots = profile::read(m_working).value();
                say(tr("Changes undone."));
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
                   tr("Mission changed."));
        } else {
            const std::uint32_t current = data.results.at(event.row);
            const std::uint32_t next = current == profile::kNoResult        ? 0
                                       : current >= profile::kHighestResult ? profile::kNoResult
                                                                            : current + 1;
            change(profile::setResult(m_working, m_slot, event.row, next), tr("Mission changed."));
        }
        return Outcome::Stay;
    }
    if (event.row >= m_backups.size()) {
        return back();
    }
    if (confirmed != event.row) {
        m_confirm = event.row;
        say(tr("Press Enter again to put this backup back. The profile as it is now is backed up first."));
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
    say(tr("Backup put back. The file it replaced is in the backups too."));
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
        if (m_leaving) {
            return Outcome::Back;  // the second Escape leaves them
        }
        say(tr("Unsaved changes. Escape again to leave without them, or open the slot and save."), true);
        m_leaving = true;
        return Outcome::Stay;
    }
    return Outcome::Back;
}

}  // namespace black::app
