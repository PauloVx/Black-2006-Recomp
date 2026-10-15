#pragma once

#include "core/ProjectLayout.h"
#include "core/Result.h"

#include <filesystem>
#include <string_view>

namespace black::core {

/// The game shows its menus from game/language/strings/MainUS.bin. Each menu language is a replacement bank under
/// local/language-banks/<language>/MainUS.bin (made by scripts/localization); the retail English bank is kept
/// there as en-US the first time another language is installed.
inline constexpr std::string_view kRetailEnglishBankSha256 =
    "B23EEB4AD86A72894B40E7239D0EA8842E5A6BC05936A936401B608070E8C7BB";

[[nodiscard]] bool isMachineDraft(std::string_view language);
[[nodiscard]] std::filesystem::path languageBankPath(const ProjectLayout& project, std::string_view language);

/// Keeps the game's retail English bank as en-US, once (true when it was kept now).
[[nodiscard]] Result<bool> preserveEnglishBank(const ProjectLayout& project,
                                               std::string_view retailEnglishSha256 = kRetailEnglishBankSha256);

/// Puts the language's bank in the game, after keeping the retail English one. Nothing changes when it is already
/// there.
[[nodiscard]] Result<bool> installLanguageBank(const ProjectLayout& project, std::string_view language,
                                               std::string_view retailEnglishSha256 = kRetailEnglishBankSha256);

}  // namespace black::core
