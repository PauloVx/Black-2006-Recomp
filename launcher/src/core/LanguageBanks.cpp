#include "core/LanguageBanks.h"

#include "core/Settings.h"
#include "core/Sha256.h"
#include "core/Text.h"

#include <algorithm>

namespace black::core {
namespace fs = std::filesystem;
namespace {

fs::path gameBankPath(const ProjectLayout& project) {
    return project.gameDirectory() / "language" / "strings" / "MainUS.bin";
}

std::string hashOf(const fs::path& path) {
    const auto digest = sha256File(path);
    return digest ? Sha256::toHex(digest.value()) : std::string();
}

/// Copies beside the target and renames over it, so the game never sees half a bank.
Result<bool> replaceFile(const fs::path& source, const fs::path& target, bool overwrite) {
    std::error_code error;
    const fs::path temporary = target.string() + ".launcher-tmp";
    fs::copy_file(source, temporary, fs::copy_options::overwrite_existing, error);
    if (!error) {
        if (!overwrite && fs::exists(target, error)) {
            fs::remove(temporary, error);
            return false;
        }
        fs::rename(temporary, target, error);
    }
    if (error) {
        fs::remove(temporary, error);
        return Error{"Could not write " + toUtf8(target)};
    }
    return true;
}

}  // namespace

bool isMachineDraft(std::string_view language) {
    return language == "es" || language == "fr" || language == "de" || language == "it";
}

fs::path languageBankPath(const ProjectLayout& project, std::string_view language) {
    return project.localDirectory() / "language-banks" / fs::path(language) / "MainUS.bin";
}

Result<bool> installLanguageBank(const ProjectLayout& project, std::string_view language,
                                 std::string_view retailEnglishSha256) {
    if (std::find(choices::kLanguages.begin(), choices::kLanguages.end(), language) == choices::kLanguages.end()) {
        return Error{"Unknown menu language " + std::string(language)};
    }
    const fs::path target = gameBankPath(project);
    const fs::path source = languageBankPath(project, language);
    std::error_code error;
    if (!fs::is_regular_file(target, error)) {
        return Error{"The game's menu text (game/language/strings/MainUS.bin) is missing."};
    }
    if (language != "en-US" && !fs::is_regular_file(source, error)) {
        return Error{"The " + std::string(language) + " menu text is missing: " + toUtf8(source)};
    }

    const fs::path english = languageBankPath(project, "en-US");
    if (!fs::is_regular_file(english, error)) {
        if (hashOf(target) != retailEnglishSha256) {
            return Error{
                "The game's MainUS.bin is not the retail English one. Put the original back (from your "
                "disc) at " +
                toUtf8(english) + " before changing the menu language."};
        }
        fs::create_directories(english.parent_path(), error);
        if (auto kept = replaceFile(target, english, false); !kept) {
            return kept.error();
        }
        if (language == "en-US") {
            return true;
        }
    }
    if (hashOf(source) == hashOf(target)) {
        return true;
    }
    return replaceFile(source, target, true);
}

}  // namespace black::core
