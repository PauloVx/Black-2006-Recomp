#include "core/ErrorReport.h"

#include "core/Text.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <fstream>
#include <regex>

namespace black::core {
namespace fs = std::filesystem;
namespace {

constexpr std::size_t kMaxErrorLines = 20;
constexpr std::size_t kMaxLineLength = 500;
constexpr std::size_t kMaxSummary = 7000;

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

void replaceIgnoringCase(std::string& line, const std::string& find, std::string_view with) {
    if (find.empty()) {
        return;
    }
    const std::string needle = lower(find);
    std::size_t at = 0;
    while ((at = lower(line).find(needle, at)) != std::string::npos) {
        line.replace(at, find.size(), with);
        at += with.size();
    }
}

std::string hidePaths(std::string line, const fs::path& root, const fs::path& home) {
    for (const auto& [folder, name] : {std::pair{root, "[game folder]"}, std::pair{home, "[user folder]"}}) {
        if (folder.empty()) {
            continue;
        }
        const std::string text = toUtf8(folder);
        replaceIgnoringCase(line, text, name);
        std::string slashed = text;
        std::replace(slashed.begin(), slashed.end(), '\\', '/');
        replaceIgnoringCase(line, slashed, name);
    }
    if (line.size() > kMaxLineLength) {
        line = line.substr(0, kMaxLineLength) + "...";
    }
    return line;
}

std::vector<std::string> errorLines(const fs::path& log) {
    static const std::regex pattern(R"(\b(error|failed|failure|exception|fatal|crash|assert)\b)", std::regex::icase);
    std::ifstream in(log, std::ios::binary);
    std::deque<std::string> found;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (std::regex_search(line, pattern)) {
            if (found.size() == kMaxErrorLines) {
                found.pop_front();
            }
            found.push_back(line);
        }
    }
    return {found.begin(), found.end()};
}

nlohmann::json readJson(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return nlohmann::json::parse(in, nullptr, false);
}

std::string languageName(const std::string& locale) {
    if (locale == "pt-BR") {
        return "Portuguese (Brazil)";
    }
    if (locale == "en-US") {
        return "English (United States)";
    }
    return "Other";
}

}  // namespace

std::optional<fs::path> latestSession(const ProjectLayout& project) {
    std::error_code error;
    std::optional<fs::path> latest;
    for (fs::directory_iterator it(project.root() / "reports", error), end; !error && it != end; it.increment(error)) {
        const std::string name = toUtf8(it->path().filename());
        if (it->is_directory(error) && name.starts_with("launcher-") &&
            (!latest || name > toUtf8(latest->filename()))) {
            latest = it->path();
        }
    }
    return latest;
}

SessionSummary summarizeSession(const ProjectLayout& project, const fs::path& session, const fs::path& home) {
    SessionSummary summary;
    summary.folderName = toUtf8(session.filename());
    std::vector<std::string> lines{"Session folder: " + summary.folderName};

    if (const nlohmann::json launch = readJson(session / "launch.json"); launch.is_object()) {
        const auto add = [&](const char* key, const char* label) {
            if (const auto it = launch.find(key); it != launch.end() && it->is_string()) {
                lines.push_back(label + it->get<std::string>());
            }
        };
        add("startedUtc", "Started (UTC): ");
        if (const auto it = launch.find("menuLanguage"); it != launch.end() && it->is_string()) {
            const std::string locale = it->get<std::string>();
            lines.push_back("Game menu language: " + languageName(locale) + " (" + locale + ")");
        }
        add("exeSha256", "Game executable SHA-256: ");
        add("xbeSha256", "Retail game SHA-256: ");
    }
    if (const nlohmann::json exit = readJson(session / "exit.json"); exit.is_object()) {
        if (const auto it = exit.find("exitCode"); it != exit.end() && it->is_number_integer()) {
            summary.exitCode = it->get<int>();
            lines.push_back("Process exit code: " + std::to_string(*summary.exitCode));
            lines.emplace_back(*summary.exitCode != 0 ? "Classification: process exited with a nonzero code."
                                                      : "Classification: no nonzero exit code was detected; please "
                                                        "describe the observed symptom under Additional notes.");
        }
    }
    for (const char* name : {"session.log", "kernel.log"}) {
        std::error_code error;
        if (!fs::is_regular_file(session / name, error)) {
            continue;
        }
        const std::vector<std::string> found = errorLines(session / name);
        if (found.empty()) {
            lines.push_back(std::string("No error-like lines were detected in ") + name + ".");
            continue;
        }
        lines.push_back(std::string("Recent error-like lines from ") + name + ":");
        for (const std::string& line : found) {
            std::string hidden = hidePaths(line, project.root(), home);
            lines.push_back("- " + hidden);
            summary.errorLines.push_back(std::move(hidden));
        }
    }
    std::vector<std::string> files;
    std::error_code error;
    for (fs::directory_iterator it(session, error), end; !error && it != end; it.increment(error)) {
        if (it->is_regular_file(error)) {
            files.push_back(toUtf8(it->path().filename()));
        }
    }
    std::sort(files.begin(), files.end());
    std::string list;
    for (const std::string& file : files) {
        list += (list.empty() ? "" : ", ") + file;
    }
    lines.push_back("Diagnostic files in this session: " + (list.empty() ? std::string("none") : list));

    for (const std::string& line : lines) {
        summary.text += (summary.text.empty() ? "" : "\n") + line;
    }
    if (summary.text.size() > kMaxSummary) {
        summary.text = summary.text.substr(0, kMaxSummary) + "\n[summary truncated]";
    }
    return summary;
}

std::vector<fs::path> sessionAttachments(const fs::path& session) {
    std::vector<fs::path> found;
    std::error_code error;
    for (const char* name :
         {"session.log", "kernel.log", "visual-trace.log", "audio-dsp-trace.log", "kgpu-draw-trace.log"}) {
        if (fs::is_regular_file(session / name, error)) {
            found.push_back(session / name);
        }
    }
    return found;
}

std::string reportBody(const std::string& summary) {
    return "## Automatically collected diagnostics\n\n" + summary +
           "\n\nLogs remain local; no files were attached automatically.\n\nAttachments:\nPlease attach the files "
           "listed in the launcher before submitting.\n\nIf you would like to add more information, write it below "
           "this line.\n\nAdditional notes:\n";
}

std::string percentEncode(std::string_view text) {
    constexpr std::string_view kHex = "0123456789ABCDEF";
    std::string encoded;
    for (const char c : text) {
        const auto byte = static_cast<unsigned char>(c);
        if (std::isalnum(byte) != 0 || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded.push_back(c);
        } else {
            encoded.push_back('%');
            encoded.push_back(kHex.at(byte >> 4U));
            encoded.push_back(kHex.at(byte & 0x0FU));
        }
    }
    return encoded;
}

std::string issueUrl(std::string_view title, std::string_view body, std::size_t maxLength) {
    constexpr std::string_view kShortened =
        "\n\n[The browser form was shortened to fit the URL. The full report is in the clipboard and local draft.]";
    const std::string prefix = std::string(kIssuesUrl) + "?title=" + percentEncode(title) + "&body=";
    const std::string whole = percentEncode(body);
    if (prefix.size() + whole.size() <= maxLength) {
        return prefix + whole;
    }
    const std::size_t room = maxLength - prefix.size() - percentEncode(kShortened).size();
    std::size_t end = 0;
    std::size_t used = 0;
    while (end < body.size()) {
        std::size_t next = end + 1;  // whole UTF-8 sequences only
        while (next < body.size() && (static_cast<unsigned char>(body[next]) & 0xC0U) == 0x80U) {
            ++next;
        }
        const std::size_t cost = percentEncode(body.substr(end, next - end)).size();
        if (used + cost > room) {
            break;
        }
        used += cost;
        end = next;
    }
    return prefix + percentEncode(std::string(body.substr(0, end)) + std::string(kShortened));
}

Result<fs::path> saveReportDraft(const ProjectLayout& project, std::string_view body) {
    const fs::path folder = project.root() / "reports" / "issue-drafts";
    std::error_code error;
    fs::create_directories(folder, error);
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    std::array<char, 32> stamp{};
    const std::size_t length = std::strftime(stamp.data(), stamp.size(), "%Y%m%d-%H%M%S", &local);
    const fs::path path = folder / ("issue-draft-" + std::string(stamp.data(), length) + ".md");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(body.data(), static_cast<std::streamsize>(body.size()));
    if (!out) {
        return Error{"Could not save the report in " + toUtf8(folder)};
    }
    return path;
}

fs::path userHome() {
#ifdef _WIN32
    wchar_t* value = nullptr;
    std::size_t length = 0;
    fs::path home;
    if (_wdupenv_s(&value, &length, L"USERPROFILE") == 0 && value != nullptr) {
        home = value;
    }
    std::free(value);  // NOLINT(cppcoreguidelines-no-malloc): _wdupenv_s allocates with malloc
    return home;
#else
    const char* value = std::getenv("HOME");  // NOLINT(concurrency-mt-unsafe)
    return value == nullptr ? fs::path() : fs::path(value);
#endif
}

}  // namespace black::core
