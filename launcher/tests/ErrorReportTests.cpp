#include "TempFolder.h"
#include "core/ErrorReport.h"

#include <doctest/doctest.h>

using namespace black::core;

TEST_CASE("Percent-encoding keeps only unreserved characters") {
    CHECK(percentEncode("a-Z_0.~") == "a-Z_0.~");
    CHECK(percentEncode("a b\n&") == "a%20b%0A%26");
    CHECK(percentEncode("\xC3\xA3") == "%C3%A3");
}

TEST_CASE("An issue link is shortened to fit, never inside a UTF-8 character") {
    const std::string url = issueUrl("Automatic report", "short body");
    CHECK(url.starts_with(std::string(kIssuesUrl) + "?title=Automatic%20report&body=short%20body"));

    std::string body;
    for (int i = 0; i < 4000; ++i) {
        body += "\xC3\xA3";  // two bytes, six once encoded
    }
    const std::string shortened = issueUrl("t", body, 2000);
    CHECK(shortened.size() <= 2000);
    CHECK(shortened.find("shortened%20to%20fit") != std::string::npos);
    const std::size_t start = shortened.find("&body=") + 6;
    CHECK(shortened.substr(start, 6) == "%C3%A3");
    CHECK(shortened.find("%C3%0A") == std::string::npos);  // no lead byte cut from its continuation
}

TEST_CASE("A session summary has the run's facts and hides personal folders") {
    const black::tests::TempFolder folder;
    const ProjectLayout project(folder.path());
    const std::string root = folder.path().string();
    folder.write("reports/launcher-20260101T000000000Z-aaaaaaaa/session.log", "old\n");
    const auto session = folder.path() / "reports/launcher-20261015T192616461Z-8701a8f7";
    folder.write("reports/launcher-20261015T192616461Z-8701a8f7/launch.json",
                 R"({"startedUtc":"2026-10-15T19:26:16Z","menuLanguage":"pt-BR","exeSha256":"AB"})");
    folder.write("reports/launcher-20261015T192616461Z-8701a8f7/exit.json", R"({"exitCode":-1})");
    folder.write("reports/launcher-20261015T192616461Z-8701a8f7/session.log",
                 "fine\r\nERROR: could not open " + root + "/game/x.bin\r\nerrors are not words\r\n");
    folder.write("reports/launcher-20261015T192616461Z-8701a8f7/kernel.log", "C:/Users/Player/x failed\n");

    const auto latest = latestSession(project);
    REQUIRE(latest.has_value());
    CHECK(latest->filename() == session.filename());

    const SessionSummary summary = summarizeSession(project, *latest, "C:\\Users\\Player");
    CHECK(summary.exitCode == -1);
    REQUIRE(summary.errorLines.size() == 2);
    CHECK(summary.errorLines[0] == "ERROR: could not open [game folder]/game/x.bin");
    CHECK(summary.errorLines[1] == "[user folder]/x failed");
    CHECK(summary.text.find("Game menu language: Portuguese (Brazil) (pt-BR)") != std::string::npos);
    CHECK(summary.text.find("nonzero") != std::string::npos);
    CHECK(summary.text.find("exit.json, kernel.log, launch.json, session.log") != std::string::npos);

    const auto attachments = sessionAttachments(*latest);
    REQUIRE(attachments.size() == 2);
    CHECK(attachments[0].filename() == "session.log");

    const auto draft = saveReportDraft(project, reportBody(summary.text));
    REQUIRE(draft.ok());
    CHECK(draft.value().parent_path().filename() == "issue-drafts");
}
