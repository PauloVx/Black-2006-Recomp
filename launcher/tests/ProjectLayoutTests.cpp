#include "TempFolder.h"
#include "core/ProjectLayout.h"

#include <doctest/doctest.h>

using black::core::ProjectLayout;

namespace {

void makeProject(const black::tests::TempFolder& folder) {
    folder.write("CMakeLists.txt", "project(Test)\n");
    folder.write("scripts/generate.py", "\n");
}

}  // namespace

TEST_CASE("A project folder has CMakeLists.txt and scripts/generate.py") {
    const black::tests::TempFolder folder;
    CHECK_FALSE(ProjectLayout::isProjectRoot(folder.path()));
    folder.write("CMakeLists.txt", "\n");
    CHECK_FALSE(ProjectLayout::isProjectRoot(folder.path()));
    folder.write("scripts/generate.py", "\n");
    CHECK(ProjectLayout::isProjectRoot(folder.path()));
}

TEST_CASE("locate searches upwards from a subfolder") {
    const black::tests::TempFolder folder;
    makeProject(folder);
    std::filesystem::create_directories(folder.path() / "build" / "bin");

    const auto found = ProjectLayout::locate(folder.path() / "build" / "bin");
    REQUIRE(found.has_value());
    CHECK(std::filesystem::equivalent(found->root(), folder.path()));
    CHECK(found->defaultXbe() == found->root() / "game" / "default.xbe");
}

TEST_CASE("locate finds nothing outside a project") {
    const black::tests::TempFolder folder;
    CHECK_FALSE(ProjectLayout::locate(folder.path()).has_value());
}

TEST_CASE("gameExecutable prefers a portable bin/ over the build tree") {
    const black::tests::TempFolder folder;
    makeProject(folder);
    const ProjectLayout project(folder.path());
    CHECK_FALSE(project.gameExecutable().has_value());

    folder.write("build-vs2022/Release/black_xbox_recomp.exe", "MZ");
    REQUIRE(project.gameExecutable().has_value());
    CHECK(project.gameExecutable()->parent_path().filename() == "Release");

    folder.write("bin/black_xbox_recomp.exe", "MZ");
    CHECK(project.gameExecutable()->parent_path().filename() == "bin");
}
