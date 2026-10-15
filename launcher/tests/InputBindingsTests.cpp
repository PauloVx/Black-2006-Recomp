#include "core/InputBindings.h"

#include <doctest/doctest.h>

namespace bindings = black::core::bindings;

TEST_CASE("Every action's default keys are valid") {
    for (const auto& action : bindings::actions()) {
        CHECK_MESSAGE(!bindings::problem(action.defaultKeys), std::string(action.name));
    }
}

TEST_CASE("problem names reserved, unknown, duplicate and too many keys") {
    CHECK_FALSE(bindings::problem("LCtrl, c"));
    CHECK_FALSE(bindings::problem("None"));
    CHECK(bindings::problem("Escape")->find("reserved") != std::string::npos);
    CHECK(bindings::problem("F11")->find("reserved") != std::string::npos);
    CHECK(bindings::problem("Banana")->find("unknown") != std::string::npos);
    CHECK(bindings::problem("C,c")->find("duplicate") != std::string::npos);
    CHECK(bindings::problem("A,B,C,D")->find("at most") != std::string::npos);
}

TEST_CASE("tokens are canonical, unique and capped") {
    CHECK(bindings::tokens(" lctrl ,C,c,mouse1,F11,X") == std::vector<std::string>{"LCtrl", "C", "Mouse1"});
    CHECK(bindings::join({}) == "None");
    CHECK(bindings::join({"Q", "WheelUp"}) == "Q,WheelUp");
}

TEST_CASE("repair keeps only real changes") {
    const bindings::Overrides repaired = bindings::repair({
        {"fire", "Mouse2"},      // changed: kept, canonical name
        {"RELOAD", "R"},         // the default: dropped
        {"USE", "None"},         // cleared on purpose: kept
        {"GRENADE", "Banana"},   // nothing usable: dropped
        {"NOT_AN_ACTION", "W"},  // dropped
    });
    CHECK(repaired == bindings::Overrides{{"FIRE", "Mouse2"}, {"USE", "None"}});
}

TEST_CASE("controls.ini keeps the game's other sections") {
    const std::string existing = "; old header\n[keyboard]\nFIRE=Mouse2\n[controller]\nlayout=2\n";
    const auto read = bindings::fromControlsIni(existing);
    REQUIRE(read.has_value());
    CHECK(read->at("FIRE") == "Mouse2");

    const std::string written = bindings::toControlsIni(existing, {{"AIM", "Mouse1"}});
    CHECK(written.find("FIRE=Mouse1\n") != std::string::npos);
    CHECK(written.find("AIM=Mouse1\n") != std::string::npos);
    CHECK(written.find("[controller]\nlayout=2\n") != std::string::npos);
    CHECK(written.find("old header") == std::string::npos);
    CHECK(bindings::fromControlsIni(written) == bindings::Overrides{{"AIM", "Mouse1"}});

    CHECK_FALSE(bindings::fromControlsIni("[controller]\nlayout=2\n"));

    const std::string again = bindings::toControlsIni(written, {{"AIM", "Mouse1"}});
    CHECK(again == written);
    CHECK(bindings::toControlsIni(again + "\n\n", {{"AIM", "Mouse1"}}) == written);
}

TEST_CASE("sharedKeys reports a key used twice") {
    CHECK(bindings::sharedKeys({}).empty());
    const auto shared = bindings::sharedKeys({{"AIM", "Mouse1"}});
    REQUIRE(shared.size() == 1);
    CHECK(std::string(shared[0].key) == "Left mouse button");
    CHECK(std::string(shared[0].first) == "Fire");
    CHECK(std::string(shared[0].second) == "Aim / zoom");
}
