#pragma once

#include <initializer_list>
#include <string>
#include <string_view>

namespace black::app {

/// The launcher speaks the game's menu language (Settings > Game). Text is looked up by its English, in the
/// catalogs under launcher/assets/translations; scripts/check-launcher-translations.py finds text that has none.
void setLanguage(std::string_view locale);

[[nodiscard]] std::string tr(std::string_view english);
/// With {0}, {1}... replaced by `values`, in the order the language puts them.
[[nodiscard]] std::string trf(std::string_view english, std::initializer_list<std::string_view> values);
/// Marks text kept in a table and translated where it is shown.
[[nodiscard]] constexpr const char* trMark(const char* english) {
    return english;
}

}  // namespace black::app
