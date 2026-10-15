#include "core/Profile.h"

#include "core/Files.h"
#include "core/Sha1.h"
#include "core/Sha256.h"
#include "core/Text.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <random>
#include <regex>

namespace black::core::profile {
namespace fs = std::filesystem;
namespace {

constexpr std::size_t kSlotSize = 0x778;
constexpr std::size_t kPayloadSize = 0x770;
constexpr std::size_t kSignatureSize = 20;
constexpr std::size_t kCampaignOffset = 0x124;
constexpr std::size_t kRecordSize = 0x20;
constexpr std::size_t kAvailableOffset = 0x1A;
constexpr std::size_t kNameOffset = 0x0D0;
constexpr std::size_t kNameSize = 0x50;
constexpr std::size_t kEmptyFlagOffset = 0x76D;
constexpr std::uint32_t kSlotMarker = 0x26;

std::uint32_t readU32(const Bytes& data, std::size_t at) {
    return std::uint32_t{data[at]} | (std::uint32_t{data[at + 1]} << 8U) | (std::uint32_t{data[at + 2]} << 16U) |
           (std::uint32_t{data[at + 3]} << 24U);
}

void writeU32(Bytes& data, std::size_t at, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        data[at + i] = static_cast<std::uint8_t>(value >> (i * 8U));
    }
}

std::uint32_t checksum(const Bytes& data, std::size_t slot) {
    std::uint32_t sum = 0;
    for (std::size_t at = slot * kSlotSize; at < slot * kSlotSize + kPayloadSize; at += 4) {
        sum += readU32(data, at);
    }
    return sum;
}

Sha1::Digest signature(const Bytes& data) {
    const std::array<std::uint8_t, 16> key{};
    const Sha1::Digest inner = hmacSha1(key, std::span(data).first(kFileSize - kSignatureSize));
    return hmacSha1(key, inner);
}

bool isActive(const Bytes& data, std::size_t slot) {
    return readU32(data, slot * kSlotSize) == kSlotMarker && data[slot * kSlotSize + kEmptyFlagOffset] == 0;
}

std::size_t recordAt(std::size_t slot, std::size_t mission) {
    return slot * kSlotSize + kCampaignOffset + mission * kRecordSize;
}

Result<Bytes> beginEdit(const Bytes& data, std::size_t slot) {
    if (const auto broken = problem(data)) {
        return Error{*broken};
    }
    if (slot >= kSlots || !isActive(data, slot)) {
        return Error{"That profile slot is empty."};
    }
    return data;
}

Result<Bytes> finishEdit(Bytes data) {
    seal(data);
    if (const auto broken = problem(data)) {
        return Error{*broken};
    }
    return data;
}

std::string timeStamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    std::array<char, 40> text{};
    const std::size_t length = std::strftime(text.data(), text.size(), "%Y%m%d_%H%M%S", &local);
    std::array<char, 8> millis{};
    std::snprintf(millis.data(), millis.size(), "_%03d",
                  static_cast<int>(ms));  // NOLINT(cppcoreguidelines-pro-type-vararg)
    return std::string(text.data(), length) + millis.data();
}

std::string randomSuffix() {
    std::random_device device;
    std::array<char, 9> text{};
    std::snprintf(text.data(), text.size(), "%08x", static_cast<unsigned>(device()));  // NOLINT
    return text.data();
}

Result<Bytes> readProfile(const fs::path& path) {
    auto bytes = readFile(path);
    if (!bytes) {
        return bytes.error();
    }
    return Bytes(bytes.value().begin(), bytes.value().end());
}

Result<fs::path> backUp(const fs::path& profile, const fs::path& folder, std::string_view prefix) {
    std::error_code error;
    fs::create_directories(folder, error);
    const fs::path backup = folder / (std::string(prefix) + timeStamp() + "_" + randomSuffix() + ".bak");
    if (!fs::copy_file(profile, backup, fs::copy_options::none, error) || error) {
        return Error{"Could not back up the profile to " + toUtf8(folder)};
    }
    return backup;
}

/// Writes beside the profile and renames over it, then reads it back to check it.
Result<bool> replaceProfile(const fs::path& profile, const Bytes& data) {
    const fs::path temporary = profile.string() + "." + randomSuffix() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));  // NOLINT
        out.flush();
        if (!out) {
            std::error_code ignored;
            fs::remove(temporary, ignored);
            return Error{"Could not write the profile."};
        }
    }
    std::error_code error;
    fs::rename(temporary, profile, error);
    if (error) {
        fs::remove(temporary, error);
        return Error{"Could not replace the profile; it was left as it was."};
    }
    const auto written = readProfile(profile);
    if (!written || written.value() != data) {
        return Error{"The profile did not read back as written. Restore its latest backup."};
    }
    return true;
}

}  // namespace

std::string sha256Of(const Bytes& data) {
    Sha256 hash;
    hash.update(std::as_bytes(std::span(data)));
    return Sha256::toHex(hash.finish());
}

std::optional<std::string> problem(const Bytes& data) {
    if (data.size() != kFileSize) {
        return "A profile is exactly 6,144 bytes.";
    }
    for (std::size_t slot = 0; slot < kSlots; ++slot) {
        const std::uint32_t marker = readU32(data, slot * kSlotSize);
        if (marker != kSlotMarker && marker != 0) {
            return "Profile slot " + std::to_string(slot + 1) + " has an unknown marker.";
        }
        if (readU32(data, slot * kSlotSize + 4) != kSlotSize) {
            return "Profile slot " + std::to_string(slot + 1) + " has an unknown size.";
        }
        if (marker == kSlotMarker && readU32(data, slot * kSlotSize + kPayloadSize) != checksum(data, slot)) {
            return "Profile slot " + std::to_string(slot + 1) + " fails its checksum.";
        }
    }
    const Sha1::Digest expected = signature(data);
    if (!std::equal(expected.begin(), expected.end(), data.end() - kSignatureSize)) {
        return "The profile's signature does not match.";
    }
    return std::nullopt;
}

void seal(Bytes& data) {
    for (std::size_t slot = 0; slot < kSlots; ++slot) {
        if (readU32(data, slot * kSlotSize) == kSlotMarker) {
            writeU32(data, slot * kSlotSize + kPayloadSize, checksum(data, slot));
        }
    }
    const Sha1::Digest sealed = signature(data);
    std::copy(sealed.begin(), sealed.end(), data.end() - kSignatureSize);
}

Result<Slots> read(const Bytes& data) {
    if (const auto broken = problem(data)) {
        return Error{*broken};
    }
    Slots slots;
    for (std::size_t slot = 0; slot < kSlots; ++slot) {
        Slot& out = slots.at(slot);
        out.active = isActive(data, slot);
        out.results.fill(kNoResult);
        if (!out.active) {
            continue;
        }
        const std::size_t name = slot * kSlotSize + kNameOffset;
        std::size_t length = 0;
        while (length < kNameSize && data[name + length] != 0) {
            const std::uint8_t c = data[name + length];
            if (c < 0x20 || c > 0x7E) {
                return Error{"Profile slot " + std::to_string(slot + 1) + " has an unreadable name."};
            }
            ++length;
        }
        if (length == kNameSize) {
            return Error{"Profile slot " + std::to_string(slot + 1) + " has a name with no end."};
        }
        out.name.assign(data.begin() + static_cast<std::ptrdiff_t>(name),
                        data.begin() + static_cast<std::ptrdiff_t>(name + length));
        for (std::size_t mission = 0; mission < kMissions; ++mission) {
            const std::size_t record = recordAt(slot, mission);
            const std::uint32_t result = readU32(data, record);
            const std::uint8_t available = data[record + kAvailableOffset];
            if ((result != kNoResult && result > kHighestResult) || available > 1) {
                return Error{"Profile slot " + std::to_string(slot + 1) + " has an unknown mission record."};
            }
            out.results.at(mission) = result;
            out.available.at(mission) = available == 1;
        }
    }
    return slots;
}

Result<Bytes> setName(const Bytes& data, std::size_t slot, std::string_view name) {
    if (name.empty() || name.size() > kMaxNameLength ||
        !std::all_of(name.begin(), name.end(), [](char c) { return c >= 0x20 && c <= 0x7E; })) {
        return Error{"A name has 1 to 79 letters, digits, spaces or punctuation (no accents)."};
    }
    auto edit = beginEdit(data, slot);
    if (!edit) {
        return edit;
    }
    Bytes bytes = std::move(edit).value();
    const std::size_t at = slot * kSlotSize + kNameOffset;
    std::fill_n(bytes.begin() + static_cast<std::ptrdiff_t>(at), kNameSize, 0);
    std::copy(name.begin(), name.end(), bytes.begin() + static_cast<std::ptrdiff_t>(at));
    return finishEdit(std::move(bytes));
}

Result<Bytes> setAvailable(const Bytes& data, std::size_t slot, std::size_t mission, bool available) {
    if (mission >= kMissions) {
        return Error{"There is no such mission."};
    }
    auto edit = beginEdit(data, slot);
    if (!edit) {
        return edit;
    }
    Bytes bytes = std::move(edit).value();
    bytes[recordAt(slot, mission) + kAvailableOffset] = available ? 1 : 0;
    return finishEdit(std::move(bytes));
}

Result<Bytes> setResult(const Bytes& data, std::size_t slot, std::size_t mission, std::uint32_t result) {
    if (mission >= kMissions || (result != kNoResult && result > kHighestResult)) {
        return Error{"There is no such mission or result."};
    }
    auto edit = beginEdit(data, slot);
    if (!edit) {
        return edit;
    }
    Bytes bytes = std::move(edit).value();
    writeU32(bytes, recordAt(slot, mission), result);
    return finishEdit(std::move(bytes));
}

Result<Bytes> unlockAll(const Bytes& data, std::size_t slot) {
    auto edit = beginEdit(data, slot);
    if (!edit) {
        return edit;
    }
    Bytes bytes = std::move(edit).value();
    for (std::size_t mission = 0; mission < kMissions; ++mission) {
        writeU32(bytes, recordAt(slot, mission), kHighestResult);
        bytes[recordAt(slot, mission) + kAvailableOffset] = 1;
    }
    return finishEdit(std::move(bytes));
}

Result<Bytes> restoreMission(const Bytes& data, const Bytes& original, std::size_t slot, std::size_t mission) {
    if (mission >= kMissions) {
        return Error{"There is no such mission."};
    }
    auto edit = beginEdit(data, slot);
    if (!edit) {
        return edit;
    }
    if (problem(original) || !isActive(original, slot)) {
        return Error{"The profile as it was read has no such slot."};
    }
    Bytes bytes = std::move(edit).value();
    const auto at = static_cast<std::ptrdiff_t>(recordAt(slot, mission));
    std::copy_n(original.begin() + at, kRecordSize, bytes.begin() + at);
    return finishEdit(std::move(bytes));
}

std::vector<fs::path> findProfiles(const ProjectLayout& project) {
    std::vector<fs::path> found;
    std::error_code error;
    const fs::path udata = project.gameDirectory() / "UDATA";
    for (fs::recursive_directory_iterator it(udata, error), end; !error && it != end; it.increment(error)) {
        if (it->is_regular_file(error) && it->file_size(error) == kFileSize) {
            if (const auto bytes = readProfile(it->path()); bytes && !problem(bytes.value())) {
                found.push_back(it->path());
            }
        }
    }
    std::sort(found.begin(), found.end());
    return found;
}

fs::path backupFolder(const ProjectLayout& project, const fs::path& profile) {
    std::string key = toUtf8(fs::absolute(profile));
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    Sha256 hash;
    hash.update(std::as_bytes(std::span(key)));
    return project.localDirectory() / "profile-backups" / ("Profile_" + Sha256::toHex(hash.finish()).substr(0, 24));
}

std::optional<std::string> backupTime(const fs::path& backup) {
    static const std::regex stamp(R"((\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2})_(\d{3}))");
    const std::string name = toUtf8(backup.filename());
    std::smatch match;
    if (!std::regex_search(name, match, stamp)) {
        return std::nullopt;
    }
    return match[1].str() + "-" + match[2].str() + "-" + match[3].str() + "  " + match[4].str() + ":" + match[5].str() +
           ":" + match[6].str();
}

std::vector<fs::path> listBackups(const fs::path& folder) {
    std::vector<std::pair<std::string, fs::path>> found;  // (time made, path)
    std::error_code error;
    for (fs::directory_iterator it(folder, error), end; !error && it != end; it.increment(error)) {
        if (it->path().extension() == ".bak" && it->is_regular_file(error)) {
            if (const auto bytes = readProfile(it->path()); bytes && !problem(bytes.value())) {
                found.emplace_back(backupTime(it->path()).value_or(""), it->path());
            }
        }
    }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second > b.second;
    });
    std::vector<fs::path> paths;
    for (auto& [time, path] : found) {
        paths.push_back(std::move(path));
    }
    return paths;
}

Result<std::optional<fs::path>> apply(const fs::path& profile, const Bytes& edited, const fs::path& backups,
                                      std::string_view expectedSha256) {
    if (const auto broken = problem(edited)) {
        return Error{"The edited profile fails its checks: " + *broken};
    }
    const auto current = readProfile(profile);
    if (!current) {
        return Error{"The profile file is gone: " + toUtf8(profile)};
    }
    if (sha256Of(current.value()) != expectedSha256) {
        return Error{"The profile changed since it was read (did the game save?). Nothing was written; read it again."};
    }
    if (current.value() == edited) {
        return std::optional<fs::path>();
    }
    auto backup = backUp(profile, backups, "Profile_");
    if (!backup) {
        return backup.error();
    }
    if (auto replaced = replaceProfile(profile, edited); !replaced) {
        return replaced.error();
    }
    return std::optional<fs::path>(std::move(backup).value());
}

Result<fs::path> restore(const fs::path& profile, const fs::path& backup, const fs::path& backups) {
    std::error_code error;
    if (!fs::equivalent(backup.parent_path(), backups, error)) {
        return Error{"That backup belongs to another profile."};
    }
    const auto bytes = readProfile(backup);
    if (!bytes || problem(bytes.value())) {
        return Error{"That backup fails the profile checks."};
    }
    auto before = backUp(profile, backups, "Profile_before_restore_");
    if (!before) {
        return before.error();
    }
    if (auto replaced = replaceProfile(profile, bytes.value()); !replaced) {
        return Error{replaced.error().message + " The file before restoring is " + toUtf8(before.value())};
    }
    return before;
}

void logAction(const ProjectLayout& project, std::string_view outcome, std::size_t slot, const fs::path& profile,
               std::string_view before, std::string_view after, std::string_view detail) {
    const auto clean = [](std::string text) {
        std::replace_if(text.begin(), text.end(), [](char c) { return c == '\t' || c == '\r' || c == '\n'; }, ' ');
        return text;
    };
    std::error_code error;
    fs::create_directories(project.localDirectory(), error);
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::array<char, 32> stamp{};
    const std::size_t length = std::strftime(stamp.data(), stamp.size(), "%Y-%m-%dT%H:%M:%SZ", &utc);
    std::ofstream log(project.localDirectory() / "profile-edit-actions.log", std::ios::binary | std::ios::app);
    log << std::string_view(stamp.data(), length) << '\t' << outcome << "\tSlot=" << slot + 1
        << "\tProfile=" << clean(toUtf8(profile)) << "\tBefore=" << before << "\tAfter=" << after
        << "\tDetail=" << clean(std::string(detail)) << '\n';
}

}  // namespace black::core::profile
