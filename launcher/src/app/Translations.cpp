#include "app/Translations.h"

#include "app/EmbeddedAssets.h"

#include <nlohmann/json.hpp>

#include <span>
#include <unordered_map>

namespace black::app {
namespace {

std::unordered_map<std::string, std::string> g_catalog;

std::span<const unsigned char> catalogFor(std::string_view locale) {
    if (locale == "pt-BR") {
        return assets::translationsPtBr();
    }
    if (locale == "es") {
        return assets::translationsEs();
    }
    if (locale == "fr") {
        return assets::translationsFr();
    }
    if (locale == "de") {
        return assets::translationsDe();
    }
    if (locale == "it") {
        return assets::translationsIt();
    }
    return {};
}

}  // namespace

void setLanguage(std::string_view locale) {
    g_catalog.clear();
    const std::span<const unsigned char> data = catalogFor(locale);
    if (data.empty()) {
        return;
    }
    const nlohmann::json catalog = nlohmann::json::parse(data.begin(), data.end(), nullptr, false);
    if (!catalog.is_object()) {
        return;
    }
    for (const auto& [english, translated] : catalog.items()) {
        if (translated.is_string() && !translated.get_ref<const std::string&>().empty()) {
            g_catalog.emplace(english, translated.get<std::string>());
        }
    }
}

std::string tr(std::string_view english) {
    const auto it = g_catalog.find(std::string(english));
    return it == g_catalog.end() ? std::string(english) : it->second;
}

std::string trf(std::string_view english, std::initializer_list<std::string_view> values) {
    std::string text = tr(english);
    std::size_t index = 0;
    for (const std::string_view value : values) {
        const std::string marker = "{" + std::to_string(index++) + "}";
        for (std::size_t at = text.find(marker); at != std::string::npos; at = text.find(marker, at + value.size())) {
            text.replace(at, marker.size(), value);
        }
    }
    return text;
}

}  // namespace black::app
