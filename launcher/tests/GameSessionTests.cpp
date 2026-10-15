#include "TempFolder.h"
#include "core/Files.h"
#include "core/GameSession.h"
#include "core/TraceLog.h"

#include <doctest/doctest.h>

using namespace black::core;

TEST_CASE("The game environment follows the settings") {
    const ProjectLayout project(std::filesystem::path("/games/black"));
    Settings settings = defaultSettings();
    settings.threads = 6;
    settings.mouseSensitivity = 1.25;
    settings.bindings = {{"FIRE", "Mouse2"}};
    auto env = gameEnvironment(project, settings, PathStyle::Native);
    CHECK(env.at("RECOMP_KELVIN_THREADS") == "6");
    CHECK(env.at("RECOMP_KELVIN_GPU") == "1");
    CHECK(env.at("RECOMP_BLACK_60HZ") == "1");
    CHECK(env.at("RECOMP_BLACK_MOUSE_SENSITIVITY") == "1.25");
    CHECK(env.at("RECOMP_BLACK_BIND_FIRE") == "Mouse2");
    CHECK(env.at("RECOMP_BLACK_BIND_CROUCH") == "LCtrl,C");
    CHECK(env.at("RECOMP_LANGUAGE") == "en-US");
    CHECK_FALSE(env.contains("RECOMP_BLACK_HZ"));
    CHECK_FALSE(env.contains("RECOMP_KELVIN_ASSIST_QUEUE"));
    CHECK_FALSE(env.contains("RECOMP_PAD_SCRIPT"));

    settings.smoothHz = 120;
    settings.internalHeight = 1440;
    settings.cameraAspect = "21:9";
    settings.advanceMenus = true;
    settings.silverWeaponMode = "force-off";
    env = gameEnvironment(project, settings, PathStyle::Native);
    CHECK(env.at("RECOMP_BLACK_HZ") == "120");
    CHECK_FALSE(env.contains("RECOMP_BLACK_60HZ"));
    CHECK(env.at("RECOMP_KELVIN_ASSIST_QUEUE") == "1");
    CHECK(env.at("RECOMP_BLACK_SILVER_WEAPONS") == "0");
    CHECK(env.at("RECOMP_PAD_SCRIPT").starts_with("20000:A/600,23000:A/600"));
}

TEST_CASE("Under Wine the game's paths are on drive Z:") {
    const ProjectLayout project(std::filesystem::path("/home/player/black"));
    const auto env = gameEnvironment(project, defaultSettings(), PathStyle::Wine);
#ifndef _WIN32
    CHECK(env.at("RECOMP_VIDEO_INI") == "Z:\\home\\player\\black\\local\\video.ini");
#endif
    CHECK(env.at("RECOMP_CONTROLS_INI").starts_with("Z:"));
    CHECK(env.at("RECOMP_CONTROLS_INI").find('/') == std::string::npos);
}

TEST_CASE("Playing needs the build and the disc") {
    const black::tests::TempFolder folder;
    const ProjectLayout project(folder.path());
    CHECK_FALSE(checkReadyToPlay(project).ok());
    folder.write("bin/black_xbox_recomp.exe", "MZ");
    CHECK(checkReadyToPlay(project).error().message.find("disc") != std::string::npos);
}

TEST_CASE("A capped trace log keeps the first lines and says how many it dropped") {
    const black::tests::TempFolder folder;
    {
        TraceLog log(folder.path() / "trace.log", 256, "[KTRACE]");
        for (int i = 0; i < 20; ++i) {
            log.write("[KTRACE] line " + std::to_string(i));
        }
        log.complete();
        log.write("after complete");
        CHECK(log.failure().empty());
    }
    const auto bytes = readFile(folder.path() / "trace.log");
    REQUIRE(bytes.ok());
    const std::string text(bytes.value().begin(), bytes.value().end());
    CHECK(text.starts_with("[KTRACE] line 0\n"));
    CHECK(text.find("[KTRACE] TRUNCATED: 12 lines discarded (limit 256 bytes).\n") != std::string::npos);
    CHECK(text.find("after complete") == std::string::npos);
}
