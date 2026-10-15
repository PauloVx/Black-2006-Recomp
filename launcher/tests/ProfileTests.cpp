#include "TempFolder.h"
#include "core/Files.h"
#include "core/Profile.h"
#include "core/Sha1.h"

#include <doctest/doctest.h>

#include <string_view>

using namespace black::core;
namespace profile = black::core::profile;

namespace {

std::string hex(const Sha1::Digest& digest) {
    constexpr std::string_view kHex = "0123456789abcdef";
    std::string text;
    for (const std::uint8_t byte : digest) {
        text += kHex.at(byte >> 4U);
        text += kHex.at(byte & 0x0FU);
    }
    return text;
}

std::span<const std::uint8_t> bytesOf(std::string_view text) {
    return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};  // NOLINT
}

void put32(profile::Bytes& data, std::size_t at, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        data[at + i] = static_cast<std::uint8_t>(value >> (i * 8U));
    }
}

/// A profile with slot 1 in use ("Player", every mission locked and unplayed) and slots 2 and 3 empty.
profile::Bytes makeProfile() {
    profile::Bytes data(profile::kFileSize, 0);
    for (std::size_t slot = 0; slot < profile::kSlots; ++slot) {
        put32(data, slot * 0x778 + 4, 0x778);
    }
    put32(data, 0, 0x26);
    const std::string_view name = "Player";
    std::copy(name.begin(), name.end(), data.begin() + 0xD0);
    for (std::size_t mission = 0; mission < profile::kMissions; ++mission) {
        put32(data, 0x124 + mission * 0x20, profile::kNoResult);
    }
    profile::seal(data);
    return data;
}

}  // namespace

TEST_CASE("SHA-1 and HMAC-SHA1 match the published test vectors") {
    Sha1 abc;
    abc.update(bytesOf("abc"));
    CHECK(hex(abc.finish()) == "a9993e364706816aba3e25717850c26c9cd0d89d");
    Sha1 empty;
    CHECK(hex(empty.finish()) == "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    Sha1 long56;
    long56.update(bytesOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
    CHECK(hex(long56.finish()) == "84983e441c3bd26ebaae4aa1f95129e5e54670f1");

    const std::array<std::uint8_t, 20> key = {0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
                                              0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b};
    CHECK(hex(hmacSha1(key, bytesOf("Hi There"))) == "b617318655057264e28bc0b6fb378c8ef146be00");
    CHECK(hex(hmacSha1(bytesOf("Jefe"), bytesOf("what do ya want for nothing?"))) ==
          "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
}

TEST_CASE("A profile is read, edited and signed again") {
    const profile::Bytes data = makeProfile();
    REQUIRE_FALSE(profile::problem(data));
    const auto slots = profile::read(data);
    REQUIRE(slots.ok());
    CHECK(slots.value()[0].active);
    CHECK(slots.value()[0].name == "Player");
    CHECK_FALSE(slots.value()[1].active);
    CHECK(slots.value()[0].results[5] == profile::kNoResult);

    const auto renamed = profile::setName(data, 0, "Kellerman");
    REQUIRE(renamed.ok());
    CHECK(profile::read(renamed.value()).value()[0].name == "Kellerman");
    CHECK_FALSE(profile::setName(data, 0, "").ok());
    CHECK_FALSE(profile::setName(data, 0, "Jo\xC3\xA3o").ok());
    CHECK_FALSE(profile::setName(data, 1, "Empty slot").ok());

    const auto unlocked = profile::unlockAll(data, 0);
    REQUIRE(unlocked.ok());
    const profile::Slot all = profile::read(unlocked.value()).value()[0];
    CHECK(std::all_of(all.available.begin(), all.available.end(), [](bool b) { return b; }));
    CHECK(all.results[31] == profile::kHighestResult);

    const auto restored = profile::restoreMission(unlocked.value(), data, 0, 31);
    REQUIRE(restored.ok());
    const profile::Slot one = profile::read(restored.value()).value()[0];
    CHECK(one.results[31] == profile::kNoResult);
    CHECK(one.results[30] == profile::kHighestResult);

    const auto set = profile::setResult(data, 0, 8, 1);
    REQUIRE(set.ok());
    CHECK(profile::read(set.value()).value()[0].results[8] == 1);
    CHECK_FALSE(profile::setResult(data, 0, 8, 3).ok());
    CHECK(profile::read(profile::setAvailable(data, 0, 9, true).value()).value()[0].available[9]);
}

TEST_CASE("A damaged profile is refused") {
    profile::Bytes data = makeProfile();
    data[0x200] ^= 1U;  // inside slot 1: its checksum no longer matches
    CHECK(profile::problem(data)->find("checksum") != std::string::npos);
    data = makeProfile();
    data[0x17F0] ^= 1U;  // outside the slots: only the signature notices
    CHECK(profile::problem(data)->find("signature") != std::string::npos);
    CHECK(profile::problem(profile::Bytes(100)).has_value());
}

TEST_CASE("Applying backs up first, and refuses a profile that changed meanwhile") {
    const black::tests::TempFolder folder;
    const ProjectLayout project(folder.path());
    const profile::Bytes original = makeProfile();
    folder.write("game/UDATA/45410083/1AAF1A0557B7/Profile",
                 std::string_view(reinterpret_cast<const char*>(original.data()), original.size()));  // NOLINT
    folder.write("game/UDATA/45410083/SaveImage.xbx", "not a profile");
    const auto found = profile::findProfiles(project);
    REQUIRE(found.size() == 1);
    const auto& path = found[0];
    const auto backups = profile::backupFolder(project, path);

    const profile::Bytes edited = profile::unlockAll(original, 0).value();
    CHECK_FALSE(profile::apply(path, edited, backups, "0000").ok());

    const auto applied = profile::apply(path, edited, backups, profile::sha256Of(original));
    REQUIRE(applied.ok());
    REQUIRE(applied.value().has_value());
    const auto written = readFile(path);
    CHECK(profile::Bytes(written.value().begin(), written.value().end()) == edited);
    CHECK(profile::listBackups(backups).size() == 1);

    const auto unchanged = profile::apply(path, edited, backups, profile::sha256Of(edited));
    REQUIRE(unchanged.ok());
    CHECK_FALSE(unchanged.value().has_value());

    const auto restored = profile::restore(path, *applied.value(), backups);
    REQUIRE(restored.ok());
    const auto back = readFile(path);
    CHECK(profile::Bytes(back.value().begin(), back.value().end()) == original);
    CHECK(profile::listBackups(backups).size() == 2);
    CHECK(profile::listBackups(backups)[0].filename().string().starts_with("Profile_before_restore_"));
    CHECK(profile::backupTime("Profile_20261015_165108_908_f0b5b9ca.bak") == "2026-10-15  16:51:08");
    CHECK_FALSE(profile::backupTime("Profile.bak"));

    profile::logAction(project, "APPLIED", 0, path, "A", "B", "unlock\tall");
    const auto log = readFile(folder.path() / "local/profile-edit-actions.log");
    REQUIRE(log.ok());
    const std::string text(log.value().begin(), log.value().end());
    CHECK(text.find("\tAPPLIED\tSlot=1\t") != std::string::npos);
    CHECK(text.find("Detail=unlock all\n") != std::string::npos);
}
