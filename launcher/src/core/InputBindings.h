#pragma once

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace black::core::bindings {

/// The keyboard/mouse model the game's pc_input.c parses (RECOMP_BLACK_BIND_<ACTION> and controls.ini [keyboard]).
/// Keys are named by their QWERTY position and stored as scancodes by the game, so a binding is a key position on
/// any layout. Escape (pause/back) and F11 (window mode) belong to the game and cannot be bound.
inline constexpr std::size_t kMaxKeysPerAction = 3;
inline constexpr double kSensitivityMin = 0.1;
inline constexpr double kSensitivityMax = 10.0;

struct Action {
    std::string_view name;  // as in RECOMP_BLACK_BIND_<name>
    std::string_view label;
    std::string_view defaultKeys;
};

struct Key {
    std::string_view token;
    int scancode;  // | 0x100 when extended; -1 mouse, -2 either side of a modifier
    std::string_view label;
};

[[nodiscard]] std::span<const Action> actions();
[[nodiscard]] std::span<const Key> keys();
[[nodiscard]] const Action* findAction(std::string_view name);
[[nodiscard]] const Key* findKey(std::string_view token);
[[nodiscard]] const Key* keyForScancode(int scancode);
[[nodiscard]] bool isReserved(std::string_view token);

/// Why a binding string ("LCtrl,C") cannot be used, or nothing when it can.
[[nodiscard]] std::optional<std::string> problem(std::string_view keys);
/// The usable tokens of a binding string, canonical case, in order, without duplicates, at most kMaxKeysPerAction.
[[nodiscard]] std::vector<std::string> tokens(std::string_view keys);
/// "None" for no keys.
[[nodiscard]] std::string join(const std::vector<std::string>& tokens);

/// Only the actions the player changed: action name -> keys ("None" when cleared on purpose).
using Overrides = std::map<std::string, std::string>;

[[nodiscard]] std::string effective(const Overrides& overrides, const Action& action);
/// Drops unknown actions and keys, and entries equal to the default.
[[nodiscard]] Overrides repair(const Overrides& overrides);

/// The [keyboard] section of controls.ini, or nothing without one.
[[nodiscard]] std::optional<Overrides> fromControlsIni(std::string_view text);
/// controls.ini with [keyboard] replaced; the game's other sections ([controller], layout) are kept.
[[nodiscard]] std::string toControlsIni(std::string_view existing, const Overrides& overrides);

struct SharedKey {
    std::string_view key;  // the key's label
    std::string_view first;
    std::string_view second;  // action labels
};
/// Pairs of actions that share a key (allowed: both happen).
[[nodiscard]] std::vector<SharedKey> sharedKeys(const Overrides& overrides);

}  // namespace black::core::bindings
