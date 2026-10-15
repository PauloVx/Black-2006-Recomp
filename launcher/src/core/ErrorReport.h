#pragma once

#include "core/ProjectLayout.h"
#include "core/Result.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace black::core {

inline constexpr std::string_view kIssuesUrl = "https://github.com/portingpete/Black-2006-Recomp/issues/new";

/// The newest reports/launcher-* folder.
[[nodiscard]] std::optional<std::filesystem::path> latestSession(const ProjectLayout& project);

struct SessionSummary {
    std::string folderName;
    std::optional<int> exitCode;
    std::vector<std::string> errorLines;  // error-like lines from session.log and kernel.log, paths hidden
    std::string text;                     // the whole summary, as it goes in the report
};

/// What a report says about a session: when it ran, how it ended, and its latest error-like lines, with the
/// project and home folders replaced so no personal paths are sent.
[[nodiscard]] SessionSummary summarizeSession(const ProjectLayout& project, const std::filesystem::path& session,
                                              const std::filesystem::path& home);

/// The logs to attach by hand, most important first.
[[nodiscard]] std::vector<std::filesystem::path> sessionAttachments(const std::filesystem::path& session);

[[nodiscard]] std::string reportBody(const std::string& summary);

/// A new-issue link with the title and body filled in. Browsers cut long links, so a body that would pass
/// `maxLength` is shortened with a note that the full report is in the clipboard and saved draft.
[[nodiscard]] std::string issueUrl(std::string_view title, std::string_view body, std::size_t maxLength = 6000);

/// Saves the report as reports/issue-drafts/issue-draft-<time>.md.
[[nodiscard]] Result<std::filesystem::path> saveReportDraft(const ProjectLayout& project, std::string_view body);

[[nodiscard]] std::string percentEncode(std::string_view text);

/// The player's home folder, hidden from reports like the project folder.
[[nodiscard]] std::filesystem::path userHome();

}  // namespace black::core
