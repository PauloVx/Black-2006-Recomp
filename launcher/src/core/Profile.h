#pragma once

#include "core/ProjectLayout.h"
#include "core/Result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace black::core::profile {

/// BLACK's save profile (game/UDATA/<title>/<save>/Profile): three slots and a signature over the whole file. Each
/// slot holds a player name and 32 campaign records, 8 missions on each of the 4 difficulties.
inline constexpr std::size_t kFileSize = 0x1800;
inline constexpr std::size_t kSlots = 3;
inline constexpr std::size_t kDifficulties = 4;
inline constexpr std::size_t kMissionsPerDifficulty = 8;
inline constexpr std::size_t kMissions = kDifficulties * kMissionsPerDifficulty;
inline constexpr std::size_t kMaxNameLength = 79;
inline constexpr std::uint32_t kNoResult = 0xFFFFFFFF;
/// Results the game writes: 0 completed (no target), 1 completed below the target, 2 target reached.
inline constexpr std::uint32_t kHighestResult = 2;

using Bytes = std::vector<std::uint8_t>;

struct Slot {
    bool active = false;
    std::string name;
    std::array<bool, kMissions> available{};
    std::array<std::uint32_t, kMissions> results{};
};

using Slots = std::array<Slot, kSlots>;

/// Why the bytes are not an intact profile (size, slot markers, checksums, signature), or nothing.
[[nodiscard]] std::optional<std::string> problem(const Bytes& data);
[[nodiscard]] Result<Slots> read(const Bytes& data);

/// Each edit checks the file and the slot first, and returns the edited file, checksummed and signed again.
[[nodiscard]] Result<Bytes> setName(const Bytes& data, std::size_t slot, std::string_view name);
[[nodiscard]] Result<Bytes> setAvailable(const Bytes& data, std::size_t slot, std::size_t mission, bool available);
[[nodiscard]] Result<Bytes> setResult(const Bytes& data, std::size_t slot, std::size_t mission, std::uint32_t result);
/// Every mission available and at its target.
[[nodiscard]] Result<Bytes> unlockAll(const Bytes& data, std::size_t slot);
/// One mission's record as it is in `original`.
[[nodiscard]] Result<Bytes> restoreMission(const Bytes& data, const Bytes& original, std::size_t slot,
                                           std::size_t mission);

/// Recomputes the active slots' checksums and the file signature.
void seal(Bytes& data);

/// Profiles under game/UDATA that pass the checks.
[[nodiscard]] std::vector<std::filesystem::path> findProfiles(const ProjectLayout& project);

/// local/profile-backups/Profile_<id>: each profile file has its own backups.
[[nodiscard]] std::filesystem::path backupFolder(const ProjectLayout& project, const std::filesystem::path& profile);
/// Intact backups, newest first.
[[nodiscard]] std::vector<std::filesystem::path> listBackups(const std::filesystem::path& folder);
/// When a backup was made, from its name ("2026-10-15  16:51:08"), or nothing for a name without the time.
[[nodiscard]] std::optional<std::string> backupTime(const std::filesystem::path& backup);

/// Writes `edited` over the profile after backing it up, unless the file changed since it was read
/// (`expectedSha256`). Returns the backup, or nothing when there was nothing to change.
[[nodiscard]] Result<std::optional<std::filesystem::path>> apply(const std::filesystem::path& profile,
                                                                 const Bytes& edited,
                                                                 const std::filesystem::path& backups,
                                                                 std::string_view expectedSha256);
/// Puts a backup back, after backing up the current file. Returns that backup.
[[nodiscard]] Result<std::filesystem::path> restore(const std::filesystem::path& profile,
                                                    const std::filesystem::path& backup,
                                                    const std::filesystem::path& backups);

/// One line in local/profile-edit-actions.log for each change, so any edit can be traced back.
void logAction(const ProjectLayout& project, std::string_view outcome, std::size_t slot,
               const std::filesystem::path& profile, std::string_view before, std::string_view after,
               std::string_view detail);

[[nodiscard]] std::string sha256Of(const Bytes& data);

}  // namespace black::core::profile
