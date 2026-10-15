#include "core/InputBindings.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace black::core::bindings {
namespace {

constexpr std::array kActions = {
    Action{"MOVE_FORWARD", "Move forward", "W"},
    Action{"MOVE_BACK", "Move back", "S"},
    Action{"MOVE_LEFT", "Move left", "A"},
    Action{"MOVE_RIGHT", "Move right", "D"},
    Action{"LOOK_LEFT", "Look left (keys)", "None"},
    Action{"LOOK_RIGHT", "Look right (keys)", "None"},
    Action{"LOOK_UP", "Look up (keys)", "None"},
    Action{"LOOK_DOWN", "Look down (keys)", "None"},
    Action{"FIRE", "Fire", "Mouse1"},
    Action{"AIM", "Aim / zoom", "Mouse2"},
    Action{"RELOAD", "Reload", "R"},
    Action{"CROUCH", "Crouch", "LCtrl,C"},
    Action{"USE", "Use / pick up", "E"},
    Action{"NEXT_WEAPON", "Switch weapon", "Q,WheelUp"},
    Action{"PREV_WEAPON", "Switch weapon (other way)", "WheelDown"},
    Action{"GRENADE", "Throw grenade", "G"},
    Action{"FIRE_MODE", "Fire mode", "B"},
    Action{"SUPPRESSOR", "Suppressor", "F"},
    Action{"MELEE", "Melee attack", "V,Mouse3"},
    Action{"HEALTH_PACK", "Use health pack", "H"},
    Action{"CYCLE_ITEM", "Cycle item (White)", "X"},
    Action{"BACK", "Back / objectives", "Tab"},
    Action{"PAUSE", "Start (pause)", "P"},
};

constexpr std::array kKeys = {
    Key{"A", 0x1E, "A"},
    Key{"B", 0x30, "B"},
    Key{"C", 0x2E, "C"},
    Key{"D", 0x20, "D"},
    Key{"E", 0x12, "E"},
    Key{"F", 0x21, "F"},
    Key{"G", 0x22, "G"},
    Key{"H", 0x23, "H"},
    Key{"I", 0x17, "I"},
    Key{"J", 0x24, "J"},
    Key{"K", 0x25, "K"},
    Key{"L", 0x26, "L"},
    Key{"M", 0x32, "M"},
    Key{"N", 0x31, "N"},
    Key{"O", 0x18, "O"},
    Key{"P", 0x19, "P"},
    Key{"Q", 0x10, "Q"},
    Key{"R", 0x13, "R"},
    Key{"S", 0x1F, "S"},
    Key{"T", 0x14, "T"},
    Key{"U", 0x16, "U"},
    Key{"V", 0x2F, "V"},
    Key{"W", 0x11, "W"},
    Key{"X", 0x2D, "X"},
    Key{"Y", 0x15, "Y"},
    Key{"Z", 0x2C, "Z"},
    Key{"1", 0x02, "1"},
    Key{"2", 0x03, "2"},
    Key{"3", 0x04, "3"},
    Key{"4", 0x05, "4"},
    Key{"5", 0x06, "5"},
    Key{"6", 0x07, "6"},
    Key{"7", 0x08, "7"},
    Key{"8", 0x09, "8"},
    Key{"9", 0x0A, "9"},
    Key{"0", 0x0B, "0"},
    Key{"F1", 0x3B, "F1"},
    Key{"F2", 0x3C, "F2"},
    Key{"F3", 0x3D, "F3"},
    Key{"F4", 0x3E, "F4"},
    Key{"F5", 0x3F, "F5"},
    Key{"F6", 0x40, "F6"},
    Key{"F7", 0x41, "F7"},
    Key{"F8", 0x42, "F8"},
    Key{"F9", 0x43, "F9"},
    Key{"F10", 0x44, "F10"},
    Key{"F12", 0x58, "F12"},
    Key{"Space", 0x39, "Space"},
    Key{"Enter", 0x1C, "Enter"},
    Key{"Tab", 0x0F, "Tab"},
    Key{"Backspace", 0x0E, "Backspace"},
    Key{"CapsLock", 0x3A, "Caps Lock"},
    Key{"Up", 0x148, "Up arrow"},
    Key{"Down", 0x150, "Down arrow"},
    Key{"Left", 0x14B, "Left arrow"},
    Key{"Right", 0x14D, "Right arrow"},
    Key{"Insert", 0x152, "Insert"},
    Key{"Delete", 0x153, "Delete"},
    Key{"Home", 0x147, "Home"},
    Key{"End", 0x14F, "End"},
    Key{"PageUp", 0x149, "Page Up"},
    Key{"PageDown", 0x151, "Page Down"},
    Key{"Shift", -2, "Shift (either)"},
    Key{"LShift", 0x2A, "Left Shift"},
    Key{"RShift", 0x36, "Right Shift"},
    Key{"Ctrl", -2, "Ctrl (either)"},
    Key{"LCtrl", 0x1D, "Left Ctrl"},
    Key{"RCtrl", 0x11D, "Right Ctrl"},
    Key{"Alt", -2, "Alt (either)"},
    Key{"LAlt", 0x38, "Left Alt"},
    Key{"RAlt", 0x138, "Right Alt"},
    Key{"Numpad0", 0x52, "Numpad 0"},
    Key{"Numpad1", 0x4F, "Numpad 1"},
    Key{"Numpad2", 0x50, "Numpad 2"},
    Key{"Numpad3", 0x51, "Numpad 3"},
    Key{"Numpad4", 0x4B, "Numpad 4"},
    Key{"Numpad5", 0x4C, "Numpad 5"},
    Key{"Numpad6", 0x4D, "Numpad 6"},
    Key{"Numpad7", 0x47, "Numpad 7"},
    Key{"Numpad8", 0x48, "Numpad 8"},
    Key{"Numpad9", 0x49, "Numpad 9"},
    Key{"NumpadEnter", 0x11C, "Numpad Enter"},
    Key{"NumpadPlus", 0x4E, "Numpad +"},
    Key{"NumpadMinus", 0x4A, "Numpad -"},
    Key{"NumpadMultiply", 0x37, "Numpad *"},
    Key{"NumpadDivide", 0x135, "Numpad /"},
    Key{"NumpadDecimal", 0x53, "Numpad ."},
    Key{"Minus", 0x0C, "-"},
    Key{"Equals", 0x0D, "="},
    Key{"LBracket", 0x1A, "["},
    Key{"RBracket", 0x1B, "]"},
    Key{"Semicolon", 0x27, ";"},
    Key{"Quote", 0x28, "'"},
    Key{"Grave", 0x29, "`"},
    Key{"Comma", 0x33, ","},
    Key{"Period", 0x34, "."},
    Key{"Slash", 0x35, "/"},
    Key{"Backslash", 0x2B, "\\"},
    Key{"Mouse1", -1, "Left mouse button"},
    Key{"Mouse2", -1, "Right mouse button"},
    Key{"Mouse3", -1, "Middle mouse button"},
    Key{"Mouse4", -1, "Mouse button 4"},
    Key{"Mouse5", -1, "Mouse button 5"},
    Key{"WheelUp", -1, "Mouse wheel up"},
    Key{"WheelDown", -1, "Mouse wheel down"},
};

bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
    return text;
}

std::vector<std::string_view> split(std::string_view text, char separator) {
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find(separator, start);
        parts.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (end == std::string_view::npos) {
            return parts;
        }
        start = end + 1;
    }
}

}  // namespace

std::span<const Action> actions() {
    return kActions;
}
std::span<const Key> keys() {
    return kKeys;
}

const Action* findAction(std::string_view name) {
    const auto it = std::find_if(kActions.begin(), kActions.end(),
                                 [name](const Action& action) { return equalsIgnoreCase(action.name, name); });
    return it == kActions.end() ? nullptr : &*it;
}

const Key* findKey(std::string_view token) {
    const auto it = std::find_if(kKeys.begin(), kKeys.end(),
                                 [token](const Key& key) { return equalsIgnoreCase(key.token, token); });
    return it == kKeys.end() ? nullptr : &*it;
}

const Key* keyForScancode(int scancode) {
    const auto it =
        std::find_if(kKeys.begin(), kKeys.end(), [scancode](const Key& key) { return key.scancode == scancode; });
    return it == kKeys.end() ? nullptr : &*it;
}

bool isReserved(std::string_view token) {
    return equalsIgnoreCase(token, "Esc") || equalsIgnoreCase(token, "Escape") || equalsIgnoreCase(token, "F11");
}

std::optional<std::string> problem(std::string_view keys) {
    std::vector<std::string_view> seen;
    for (const std::string_view raw : split(keys, ',')) {
        const std::string_view token = trim(raw);
        if (token.empty() || equalsIgnoreCase(token, "None")) {
            continue;
        }
        if (isReserved(token)) {
            return std::string(token) + " is reserved (Escape pauses, F11 toggles the window)";
        }
        const Key* key = findKey(token);
        if (key == nullptr) {
            return "unknown key " + std::string(token);
        }
        if (std::find(seen.begin(), seen.end(), key->token) != seen.end()) {
            return "duplicate key " + std::string(key->token);
        }
        seen.push_back(key->token);
    }
    if (seen.size() > kMaxKeysPerAction) {
        return "at most " + std::to_string(kMaxKeysPerAction) + " keys per action";
    }
    return std::nullopt;
}

std::vector<std::string> tokens(std::string_view keys) {
    std::vector<std::string> result;
    for (const std::string_view raw : split(keys, ',')) {
        const std::string_view token = trim(raw);
        const Key* key = isReserved(token) ? nullptr : findKey(token);
        if (key != nullptr && result.size() < kMaxKeysPerAction &&
            std::find(result.begin(), result.end(), key->token) == result.end()) {
            result.emplace_back(key->token);
        }
    }
    return result;
}

std::string join(const std::vector<std::string>& tokens) {
    std::string joined;
    for (const std::string& token : tokens) {
        joined += (joined.empty() ? "" : ",") + token;
    }
    return joined.empty() ? "None" : joined;
}

std::string effective(const Overrides& overrides, const Action& action) {
    for (const auto& [name, keys] : overrides) {
        if (equalsIgnoreCase(name, action.name)) {
            return join(tokens(keys));
        }
    }
    return join(tokens(action.defaultKeys));
}

Overrides repair(const Overrides& overrides) {
    Overrides repaired;
    for (const auto& [name, keys] : overrides) {
        const Action* action = findAction(name);
        if (action == nullptr) {
            continue;
        }
        const std::string joined = join(tokens(keys));
        const std::string_view written = trim(keys);
        const bool clearedOnPurpose = written.empty() || equalsIgnoreCase(written, "None");
        if ((joined == "None" && !clearedOnPurpose) || joined == join(tokens(action->defaultKeys))) {
            continue;  // nothing usable, or the default: keep the default
        }
        repaired[std::string(action->name)] = joined;
    }
    return repaired;
}

std::optional<Overrides> fromControlsIni(std::string_view text) {
    std::optional<Overrides> found;
    bool keyboard = false;
    for (const std::string_view raw : split(text, '\n')) {
        const std::string_view line = trim(raw);
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }
        if (line.front() == '[') {
            keyboard = equalsIgnoreCase(line, "[keyboard]");
            if (keyboard && !found) {
                found.emplace();
            }
            continue;
        }
        const std::size_t equals = line.find('=');
        if (!keyboard || equals == std::string_view::npos || equals == 0) {
            continue;
        }
        if (const Action* action = findAction(trim(line.substr(0, equals)))) {
            (*found)[std::string(action->name)] = std::string(trim(line.substr(equals + 1)));
        }
    }
    if (found) {
        return repair(*found);
    }
    return std::nullopt;
}

std::string toControlsIni(std::string_view existing, const Overrides& overrides) {
    std::string kept;
    bool skipping = false;
    bool any = false;
    for (std::string_view line : split(existing, '\n')) {
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        const std::string_view trimmed = trim(line);
        if (!trimmed.empty() && trimmed.front() == '[') {
            skipping = equalsIgnoreCase(trimmed, "[keyboard]");
        }
        // The header comment is written again; leading blank or comment lines of the old file are dropped
        if (skipping || (!any && (trimmed.empty() || trimmed.front() == ';'))) {
            continue;
        }
        any = true;
        kept.append(line).push_back('\n');
    }
    while (kept.ends_with("\n\n")) {
        kept.pop_back();  // or each save adds a blank line at the end
    }
    std::string text = "; BLACK controls. Edited by the in-game Controls pages and by the launcher.\n[keyboard]\n";
    for (const Action& action : kActions) {
        text.append(action.name).append("=").append(effective(overrides, action)).push_back('\n');
    }
    return text + kept;
}

std::vector<SharedKey> sharedKeys(const Overrides& overrides) {
    std::vector<SharedKey> lines;
    for (std::size_t a = 0; a < kActions.size(); ++a) {
        const std::vector<std::string> mine = tokens(effective(overrides, kActions[a]));
        for (const std::string& token : mine) {
            for (std::size_t b = a + 1; b < kActions.size(); ++b) {
                const std::vector<std::string> theirs = tokens(effective(overrides, kActions[b]));
                if (std::find(theirs.begin(), theirs.end(), token) != theirs.end()) {
                    lines.push_back({findKey(token)->label, kActions[a].label, kActions[b].label});
                }
            }
        }
    }
    return lines;
}

}  // namespace black::core::bindings
