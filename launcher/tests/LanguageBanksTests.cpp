#include "TempFolder.h"
#include "core/Files.h"
#include "core/LanguageBanks.h"
#include "core/Sha256.h"

#include <doctest/doctest.h>

using namespace black::core;

namespace {

std::string text(const std::filesystem::path& path) {
    const auto bytes = readFile(path);
    return bytes ? std::string(bytes.value().begin(), bytes.value().end()) : std::string("<missing>");
}

std::string hashOf(std::string_view data) {
    Sha256 hash;
    hash.update(std::as_bytes(std::span(data)));
    return Sha256::toHex(hash.finish());
}

}  // namespace

TEST_CASE("Changing the menu language keeps the retail English bank first") {
    const black::tests::TempFolder folder;
    const ProjectLayout project(folder.path());
    folder.write("game/language/strings/MainUS.bin", "english");
    folder.write("local/language-banks/pt-BR/MainUS.bin", "portugues");
    const std::string retail = hashOf("english");
    const auto game = folder.path() / "game/language/strings/MainUS.bin";

    REQUIRE(installLanguageBank(project, "pt-BR", retail).ok());
    CHECK(text(game) == "portugues");
    CHECK(text(languageBankPath(project, "en-US")) == "english");

    REQUIRE(installLanguageBank(project, "en-US", retail).ok());
    CHECK(text(game) == "english");

    CHECK_FALSE(installLanguageBank(project, "fr", retail).ok());  // no French bank
    CHECK_FALSE(installLanguageBank(project, "xx", retail).ok());
}

TEST_CASE("An unrecognised game bank is never kept as English") {
    const black::tests::TempFolder folder;
    const ProjectLayout project(folder.path());
    folder.write("game/language/strings/MainUS.bin", "modified");
    folder.write("local/language-banks/pt-BR/MainUS.bin", "portugues");
    const auto installed = installLanguageBank(project, "pt-BR", hashOf("english"));
    REQUIRE_FALSE(installed.ok());
    CHECK(installed.error().message.find("retail English") != std::string::npos);
    CHECK(text(folder.path() / "game/language/strings/MainUS.bin") == "modified");
}
