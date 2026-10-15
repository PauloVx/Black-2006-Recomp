#include "core/Process.h"

#include "core/Text.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// after windows.h, which it depends on
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;  // NOLINT(readability-redundant-declaration): POSIX declares it nowhere in a header
#endif

namespace black::core {
namespace {

#ifdef _WIN32
/// One command-line argument quoted the way CommandLineToArgvW reads it back.
std::wstring quoteArgument(const std::wstring& argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\"") == std::wstring::npos) {
        return argument;
    }
    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t c : argument) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        quoted.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(c);
    }
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::string lastErrorText(const char* what) {
    return std::string(what) + " (Windows error " + std::to_string(GetLastError()) + ")";
}
#endif

}  // namespace

Result<bool> startDetached(const std::filesystem::path& program, const std::vector<std::string>& arguments,
                           const std::filesystem::path& workingDirectory) {
#ifdef _WIN32
    std::wstring commandLine = quoteArgument(program.wstring());
    for (const std::string& argument : arguments) {
        commandLine += L' ';
        commandLine += quoteArgument(pathFromUtf8(argument).wstring());
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION process{};
    const std::wstring directory = workingDirectory.wstring();
    if (CreateProcessW(program.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT,
                       nullptr, directory.empty() ? nullptr : directory.c_str(), &startup, &process) == 0) {
        return Error{lastErrorText(("Could not start " + toUtf8(program)).c_str())};
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
#else
    // A double fork, so the program is not the launcher's child: it keeps running and is never left a zombie.
    const std::string file = program.string();
    const std::string directory = workingDirectory.string();
    std::vector<std::string> owned{file};
    owned.insert(owned.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    for (std::string& argument : owned) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    const pid_t child = fork();
    if (child < 0) {
        return Error{"Could not start " + file};
    }
    if (child == 0) {
        setsid();
        if (!directory.empty() && chdir(directory.c_str()) != 0) {
            _exit(127);
        }
        pid_t grandchild = 0;
        _exit(posix_spawnp(&grandchild, file.c_str(), nullptr, nullptr, argv.data(), environ) == 0 ? 0 : 127);
    }
    int status = 0;
    waitpid(child, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return Error{"Could not start " + file};
    }
    return true;
#endif
}

Result<bool> openFolder(const std::filesystem::path& folder) {
#ifdef _WIN32
    const auto result = reinterpret_cast<INT_PTR>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): Win32 API
        ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        return Error{"Could not open " + toUtf8(folder)};
    }
    return true;
#else
    return startDetached("xdg-open", {folder.string()}, {});
#endif
}

Result<bool> openUrl(const std::string& url) {
#ifdef _WIN32
    const auto result = reinterpret_cast<INT_PTR>(  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): Win32 API
        ShellExecuteW(nullptr, L"open", pathFromUtf8(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) {
        return Error{"Could not open the web browser"};
    }
    return true;
#else
    return startDetached("xdg-open", {url}, {});
#endif
}

}  // namespace black::core
