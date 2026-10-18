#include "TempFolder.h"
#include "app/SettingsFlow.h"
#include "app/Translations.h"
#include "core/Settings.h"

#include <doctest/doctest.h>

#include <algorithm>

namespace {

std::size_t row(const black::app::Page& page, std::string_view label) {
    const auto found =
        std::find_if(page.rows.begin(), page.rows.end(), [label](const auto& item) { return item.label == label; });
    REQUIRE(found != page.rows.end());
    return static_cast<std::size_t>(found - page.rows.begin());
}

}  // namespace

TEST_CASE("Gameplay settings own the opt-in crosshair and hit markers") {
    const black::tests::TempFolder folder;
    folder.write("CMakeLists.txt", "\n");
    folder.write("scripts/generate.py", "\n");
    const auto project = black::core::ProjectLayout::locate(folder.path());
    REQUIRE(project.has_value());
    black::app::setLanguage("en-US");
    black::app::SettingsFlow flow(*project);
    auto page = flow.page();
    flow.handle({row(page, "DISPLAY"), 0});
    page = flow.page();
    CHECK(std::none_of(page.rows.begin(), page.rows.end(),
                       [](const auto& item) { return item.label == "CROSSHAIR" || item.label == "HIT MARKERS"; }));
    flow.back();
    page = flow.page();
    flow.handle({row(page, "GAMEPLAY"), 0});
    page = flow.page();
    CHECK(page.title == "GAMEPLAY");
    const auto crosshair = row(page, "CROSSHAIR");
    const auto markers = row(page, "HIT MARKERS");
    CHECK(page.rows[crosshair].value == "OFF");
    CHECK(page.rows[markers].value == "OFF");
    flow.handle({markers, 1});
    CHECK(flow.page().rows[crosshair].value == "OFF");
    CHECK(flow.page().rows[markers].value == "ON");
    flow.handle({crosshair, 1});
    CHECK(flow.page().rows[crosshair].value == "CROSS");
    flow.handle({markers, 1});
    CHECK(flow.page().rows[markers].value == "OFF");
    const auto loaded = black::core::loadSettings(*project);
    CHECK(loaded.crosshair == "cross");
    CHECK(loaded.hitmarkers == "off");
    black::app::SettingsFlow reopened(*project);
    reopened.handle({row(reopened.page(), "GAMEPLAY"), 0});
    CHECK(reopened.page().rows[crosshair].value == "CROSS");
    CHECK(reopened.page().rows[markers].value == "OFF");
    reopened.handle({crosshair, -1});
    CHECK(reopened.page().rows[crosshair].value == "OFF");
}
