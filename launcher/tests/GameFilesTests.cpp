#include "TempFolder.h"
#include "core/GameFiles.h"

#include <doctest/doctest.h>

using black::core::GameFilesState;
using black::core::inspectGameFiles;

TEST_CASE("No default.xbe means the disc has not been chosen yet") {
    const black::tests::TempFolder folder;
    CHECK(inspectGameFiles(folder.path() / "game" / "default.xbe").state == GameFilesState::Missing);
}

TEST_CASE("A default.xbe from another release is reported with its hash") {
    const black::tests::TempFolder folder;
    folder.write("game/default.xbe", "XBEH not the retail release");
    const auto status = inspectGameFiles(folder.path() / "game" / "default.xbe");
    CHECK(status.state == GameFilesState::UnsupportedVersion);
    CHECK(status.sha256.size() == 64);
    CHECK(status.sha256 != black::core::kSupportedXbeSha256);
}

TEST_CASE("A folder named default.xbe is not a game file") {
    const black::tests::TempFolder folder;
    std::filesystem::create_directories(folder.path() / "game" / "default.xbe");
    CHECK(inspectGameFiles(folder.path() / "game" / "default.xbe").state == GameFilesState::Missing);
}
