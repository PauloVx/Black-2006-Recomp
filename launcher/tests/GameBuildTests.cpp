#include "TempFolder.h"
#include "core/GameBuild.h"
#include "core/Toolchain.h"

#include <doctest/doctest.h>

#include <chrono>

using namespace black::core;

TEST_CASE("Versions are read from the tools' own words") {
    CHECK(parseVersion("cmake version 3.26.4\n\nCMake suite maintained") == std::array{3, 26, 4});
    CHECK(parseVersion("Python 3.10.11") == std::array{3, 10, 11});
    CHECK(parseVersion("cargo 1.98.1 (abc 2026-01-01)") == std::array{1, 98, 1});
    CHECK(parseVersion("17.14") == std::array{17, 14, 0});
    CHECK_FALSE(parseVersion("no version here"));
    CHECK(atLeast({3, 21, 0}, 3, 21));
    CHECK(atLeast({4, 0, 0}, 3, 21));
    CHECK_FALSE(atLeast({3, 9, 9}, 3, 10));
}

TEST_CASE("winget installs the required tools that are missing, and nothing else") {
    Toolchain toolchain;
    toolchain.tools = {
        Tool{"cmake", "CMAKE", "", true, true, "3.26.4", "cmake.exe", "Kitware.CMake", {"--silent"}},
        Tool{"python", "PYTHON", "", true, false, "", "", "Python.Python.3.12", {"--silent"}},
        Tool{"git", "GIT", "", false, false, "", "", "Git.Git", {"--silent"}},
    };
    CHECK_FALSE(toolchain.ready());
    CHECK(installCommands(toolchain).empty());  // no winget

    toolchain.canInstall = true;
    const auto commands = installCommands(toolchain);
    REQUIRE(commands.size() == 1);
    CHECK(commands[0].tool == "PYTHON");
    CHECK(commands[0].program == "winget");
    const auto& arguments = commands[0].arguments;
    CHECK(std::find(arguments.begin(), arguments.end(), "Python.Python.3.12") != arguments.end());
    CHECK(arguments.back() == "--silent");

    toolchain.tools[1].found = true;
    CHECK(toolchain.ready());
    CHECK(installCommands(toolchain).empty());
}

TEST_CASE("Build output is turned into progress") {
    CHECK(generateFraction("Running tools.xbe_parser") == doctest::Approx(0.02f));
    CHECK(generateFraction("Running tools.recomp") == doctest::Approx(0.55f));
    CHECK(generateFraction("Running tools.recomp.icall_feedback") == doctest::Approx(0.25f));
    CHECK(generateFraction("Generated 18 source chunks from your XBE. Local analysis: x") == doctest::Approx(1.0f));
    CHECK_FALSE(generateFraction("Running tools.recompiler"));
    CHECK_FALSE(generateFraction("something else"));

    CHECK(compiledFileLine("  recomp_0003.c"));
    CHECK(compiledFileLine("pc_video.cpp"));
    CHECK_FALSE(compiledFileLine("  Generating Code..."));
    CHECK_FALSE(compiledFileLine("  black_xbox_recomp.vcxproj -> C:\\x\\black_xbox_recomp.exe"));

    CHECK(ninjaFraction("[12/48] Building C object src/foo.c.obj") == doctest::Approx(0.25f));
    CHECK_FALSE(ninjaFraction("-- Configuring done"));
}

TEST_CASE("Translated code is current until the disc or the translator's inputs change") {
    const black::tests::TempFolder folder;
    const ProjectLayout project(folder.path());
    CHECK_FALSE(generatedCodeIsCurrent(project));

    folder.write("game/default.xbe", "xbe");
    folder.write("scripts/generate.py", "py");
    folder.write("src/recomp/gen/post_transform_audit.json", "{}");
    const auto audit = folder.path() / "src/recomp/gen/post_transform_audit.json";
    const auto later = std::filesystem::last_write_time(audit) + std::chrono::hours(1);
    std::filesystem::last_write_time(audit, later);
    CHECK(generatedCodeIsCurrent(project));

    std::filesystem::last_write_time(folder.path() / "scripts/generate.py", later + std::chrono::hours(1));
    CHECK_FALSE(generatedCodeIsCurrent(project));
}

TEST_CASE("Translated code is out of date when the recompiler changes, not its tests") {
    const black::tests::TempFolder folder;
    const ProjectLayout project(folder.path());
    folder.write("third_party/xboxrecomp/tools/recomp/lifter.py", "py");
    folder.write("third_party/xboxrecomp/tools/recomp/test_lifter.py", "py");
    folder.write("src/recomp/gen/post_transform_audit.json", "{}");
    const auto audit = folder.path() / "src/recomp/gen/post_transform_audit.json";
    const auto later = std::filesystem::last_write_time(audit) + std::chrono::hours(1);
    std::filesystem::last_write_time(audit, later);
    CHECK(generatedCodeIsCurrent(project));

    const auto tools = folder.path() / "third_party/xboxrecomp/tools/recomp";
    std::filesystem::last_write_time(tools / "test_lifter.py", later + std::chrono::hours(1));
    CHECK(generatedCodeIsCurrent(project));
    std::filesystem::last_write_time(tools / "lifter.py", later + std::chrono::hours(1));
    CHECK_FALSE(generatedCodeIsCurrent(project));
}
