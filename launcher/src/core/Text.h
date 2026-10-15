#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace black::core {

/// A path as UTF-8 text for display and logs (C++20's u8string() returns std::u8string).
[[nodiscard]] inline std::string toUtf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return {text.begin(), text.end()};
}

[[nodiscard]] inline std::filesystem::path pathFromUtf8(const std::string& text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

/// UTF-8 to UTF-16 code units (the game's fonts are indexed by UTF-16). Malformed bytes become U+FFFD and code
/// points outside the Basic Multilingual Plane become surrogate pairs.
[[nodiscard]] std::u16string utf8ToUtf16(std::string_view text);

}  // namespace black::core
