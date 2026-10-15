#include "core/ChildProcess.h"

#include "core/Text.h"

#include <array>
#include <mutex>
#include <optional>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <cstring>

extern char** environ;  // NOLINT(readability-redundant-declaration): POSIX declares it nowhere in a header
#endif

namespace black::core {
namespace {

bool startsWithAny(std::string_view text, const std::vector<std::string>& prefixes) {
    for (const std::string& prefix : prefixes) {
        if (text.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), text.begin(), [](char a, char b) {
                return std::toupper(static_cast<unsigned char>(a)) == std::toupper(static_cast<unsigned char>(b));
            })) {
            return true;
        }
    }
    return false;
}

/// Whether `options` sets this variable (names ignore case on Windows, as Windows does).
bool overridden(const ChildOptions& options, std::string_view name) {
#ifdef _WIN32
    for (const auto& [key, value] : options.environment) {
        if (key.size() == name.size() && startsWithAny(name, {key})) {
            return true;
        }
    }
    return false;
#else
    return options.environment.contains(std::string(name));
#endif
}

/// Splits a byte stream into lines (LF or CRLF) and delivers them.
class LineSplitter {
public:
    LineSplitter(const std::function<void(std::string_view, bool)>& onLine, bool isError)
        : m_onLine(onLine), m_isError(isError) {}

    void feed(const char* data, std::size_t size) {
        for (std::size_t i = 0; i < size; ++i) {
            if (data[i] == '\n') {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                flush();
            } else {
                m_line.push_back(data[i]);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            }
        }
    }

    void flush() {
        if (!m_line.empty() && m_line.back() == '\r') {
            m_line.pop_back();
        }
        if (m_onLine) {
            m_onLine(m_line, m_isError);
        }
        m_line.clear();
    }

    void finish() {
        if (!m_line.empty()) {
            flush();
        }
    }

private:
    const std::function<void(std::string_view, bool)>& m_onLine;
    bool m_isError;
    std::string m_line;
};

}  // namespace

#ifdef _WIN32

struct ChildProcess::Impl {
    HANDLE process = nullptr;
    HANDLE job = nullptr;  // with killTree
    DWORD pid = 0;
    std::function<void(std::string_view, bool)> onLine;
    std::array<std::thread, 2> readers;
    std::optional<int> exitCode;
    std::mutex waitMutex;
    std::mutex handleMutex;  // kill() never uses the handle once wait() closed it

    void read(HANDLE pipe, bool isError) {
        LineSplitter lines(onLine, isError);
        std::array<char, 4096> buffer{};
        DWORD got = 0;
        while (ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &got, nullptr) != 0 && got > 0) {
            lines.feed(buffer.data(), got);
        }
        lines.finish();
        CloseHandle(pipe);
    }
};

namespace {

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

std::wstring wide(const std::string& text) {
    return pathFromUtf8(text).wstring();
}

std::wstring environmentBlock(const ChildOptions& options) {
    std::wstring block;
    LPWCH inherited = GetEnvironmentStringsW();
    for (const wchar_t* entry = inherited; *entry != L'\0'; entry += wcslen(entry) + 1) {  // NOLINT
        const std::wstring_view text(entry);
        const std::string name = toUtf8(std::filesystem::path(text.substr(0, text.find(L'=', 1))));
        if (startsWithAny(name, options.dropPrefixes) || overridden(options, name)) {
            continue;
        }
        block.append(text).push_back(L'\0');
    }
    FreeEnvironmentStringsW(inherited);
    for (const auto& [name, value] : options.environment) {
        block.append(wide(name)).append(L"=").append(wide(value)).push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

}  // namespace

Result<std::unique_ptr<ChildProcess>> ChildProcess::start(ChildOptions options) {
    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    std::array<HANDLE, 2> readEnds{};
    std::array<HANDLE, 2> writeEnds{};
    for (std::size_t i = 0; i < 2; ++i) {
        if (CreatePipe(&readEnds.at(i), &writeEnds.at(i), &inherit, 0) == 0) {
            return Error{"Could not start " + toUtf8(options.program) + ": no pipe for its output"};
        }
        SetHandleInformation(readEnds.at(i), HANDLE_FLAG_INHERIT, 0);
    }
    HANDLE input =
        CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, OPEN_EXISTING, 0, nullptr);

    std::wstring commandLine = quoteArgument(options.program.wstring());
    for (const std::string& argument : options.arguments) {
        commandLine += L' ';
        commandLine += quoteArgument(wide(argument));
    }
    std::wstring environment = environmentBlock(options);
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input;
    startup.hStdOutput = writeEnds[0];
    startup.hStdError = writeEnds[1];
    PROCESS_INFORMATION info{};
    const std::wstring directory = options.workingDirectory.wstring();
    // Without a folder, the program is searched for on PATH like any command
    const wchar_t* application = options.program.has_parent_path() ? options.program.c_str() : nullptr;
    HANDLE job = nullptr;
    if (options.killTree) {
        job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof limits);
    }
    // Suspended until it is in its job, so nothing it starts escapes the tree
    const BOOL started =
        CreateProcessW(application, commandLine.data(), nullptr, nullptr, TRUE,
                       CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW | (job != nullptr ? CREATE_SUSPENDED : 0),
                       environment.data(), directory.empty() ? nullptr : directory.c_str(), &startup, &info);
    const DWORD error = GetLastError();
    for (HANDLE handle : writeEnds) {
        CloseHandle(handle);
    }
    if (input != INVALID_HANDLE_VALUE) {
        CloseHandle(input);
    }
    if (started == 0) {
        for (HANDLE handle : readEnds) {
            CloseHandle(handle);
        }
        if (job != nullptr) {
            CloseHandle(job);
        }
        return Error{"Could not start " + toUtf8(options.program) + " (Windows error " + std::to_string(error) + ")"};
    }
    if (job != nullptr) {
        AssignProcessToJobObject(job, info.hProcess);
        ResumeThread(info.hThread);
    }
    CloseHandle(info.hThread);

    auto impl = std::make_unique<Impl>();
    impl->process = info.hProcess;
    impl->job = job;
    impl->pid = info.dwProcessId;
    impl->onLine = std::move(options.onLine);
    Impl* raw = impl.get();
    impl->readers[0] = std::thread([raw, pipe = readEnds[0]] { raw->read(pipe, false); });
    impl->readers[1] = std::thread([raw, pipe = readEnds[1]] { raw->read(pipe, true); });
    return std::unique_ptr<ChildProcess>(new ChildProcess(std::move(impl)));
}

int ChildProcess::pid() const noexcept {
    return static_cast<int>(m_impl->pid);
}

int ChildProcess::wait() {
    const std::scoped_lock lock(m_impl->waitMutex);
    if (!m_impl->exitCode) {
        WaitForSingleObject(m_impl->process, INFINITE);
        if (m_impl->job != nullptr) {
            TerminateJobObject(m_impl->job, 1);  // helpers left behind would hold the output open
        }
        for (std::thread& reader : m_impl->readers) {
            reader.join();
        }
        const std::scoped_lock guard(m_impl->handleMutex);
        DWORD code = 0;
        GetExitCodeProcess(m_impl->process, &code);
        CloseHandle(m_impl->process);
        m_impl->process = nullptr;
        if (m_impl->job != nullptr) {
            CloseHandle(m_impl->job);
            m_impl->job = nullptr;
        }
        m_impl->exitCode = static_cast<int>(code);
    }
    return *m_impl->exitCode;
}

void ChildProcess::kill() {
    const std::scoped_lock guard(m_impl->handleMutex);
    if (m_impl->job != nullptr) {
        TerminateJobObject(m_impl->job, 1);
    } else if (m_impl->process != nullptr) {
        TerminateProcess(m_impl->process, 1);
    }
}

#else

struct ChildProcess::Impl {
    pid_t pid = 0;
    bool tree = false;  // the child leads its own process group
    std::function<void(std::string_view, bool)> onLine;
    std::array<std::thread, 2> readers;
    std::optional<int> exitCode;
    std::mutex waitMutex;
    std::mutex pidMutex;  // kill() never signals the pid once wait() reaped it (it may be reused)

    void read(int pipe, bool isError) {
        LineSplitter lines(onLine, isError);
        std::array<char, 4096> buffer{};
        while (true) {
            const ssize_t got = ::read(pipe, buffer.data(), buffer.size());
            if (got < 0 && errno == EINTR) {
                continue;
            }
            if (got <= 0) {
                break;
            }
            lines.feed(buffer.data(), static_cast<std::size_t>(got));
        }
        lines.finish();
        close(pipe);
    }
};

Result<std::unique_ptr<ChildProcess>> ChildProcess::start(ChildOptions options) {
    std::array<std::array<int, 2>, 2> pipes{};
    for (auto& pipe : pipes) {
        if (::pipe(pipe.data()) != 0) {
            return Error{"Could not start " + options.program.string() + ": no pipe for its output"};
        }
    }
    std::vector<std::string> variables;
    for (char** entry = environ; *entry != nullptr;
         ++entry) {  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        const std::string_view text(*entry);
        const std::string name(text.substr(0, text.find('=')));
        if (!startsWithAny(name, options.dropPrefixes) && !overridden(options, name)) {
            variables.emplace_back(text);
        }
    }
    for (const auto& [name, value] : options.environment) {
        variables.push_back(name + "=" + value);
    }
    std::vector<char*> envp;
    for (std::string& variable : variables) {
        envp.push_back(variable.data());
    }
    envp.push_back(nullptr);
    std::vector<std::string> owned{options.program.string()};
    owned.insert(owned.end(), options.arguments.begin(), options.arguments.end());
    std::vector<char*> argv;
    for (std::string& argument : owned) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    // posix_spawn cannot change directory portably before glibc 2.29's addchdir_np, so fork it is
    const std::string directory = options.workingDirectory.string();
    const pid_t child = fork();
    if (child == 0) {
        if (options.killTree) {
            setpgid(0, 0);
        }
        dup2(pipes[0][1], STDOUT_FILENO);
        dup2(pipes[1][1], STDERR_FILENO);
        for (const auto& pipe : pipes) {
            close(pipe[0]);
            close(pipe[1]);
        }
        if (!directory.empty() && chdir(directory.c_str()) != 0) {
            _exit(127);
        }
        execvpe(argv[0], argv.data(), envp.data());
        _exit(127);
    }
    for (const auto& pipe : pipes) {
        close(pipe[1]);
    }
    if (child < 0) {
        for (const auto& pipe : pipes) {
            close(pipe[0]);
        }
        return Error{"Could not start " + options.program.string() + ": " + std::strerror(errno)};
    }
    auto impl = std::make_unique<Impl>();
    impl->pid = child;
    impl->tree = options.killTree;
    if (options.killTree) {
        setpgid(child, child);  // also here, so a kill() before the child runs reaches the group
    }
    impl->onLine = std::move(options.onLine);
    Impl* raw = impl.get();
    impl->readers[0] = std::thread([raw, pipe = pipes[0][0]] { raw->read(pipe, false); });
    impl->readers[1] = std::thread([raw, pipe = pipes[1][0]] { raw->read(pipe, true); });
    return std::unique_ptr<ChildProcess>(new ChildProcess(std::move(impl)));
}

int ChildProcess::pid() const noexcept {
    return static_cast<int>(m_impl->pid);
}

int ChildProcess::wait() {
    const std::scoped_lock lock(m_impl->waitMutex);
    if (!m_impl->exitCode) {
        siginfo_t info{};
        while (waitid(P_PID, static_cast<id_t>(m_impl->pid), &info, WEXITED | WNOWAIT) < 0 && errno == EINTR) {
        }
        if (m_impl->tree) {
            ::kill(-m_impl->pid, SIGKILL);  // helpers left behind would hold the output open
        }
        for (std::thread& reader : m_impl->readers) {
            reader.join();
        }
        const std::scoped_lock guard(m_impl->pidMutex);
        int status = 0;
        while (waitpid(m_impl->pid, &status, 0) < 0 && errno == EINTR) {
        }
        m_impl->pid = 0;
        m_impl->exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    }
    return *m_impl->exitCode;
}

void ChildProcess::kill() {
    const std::scoped_lock guard(m_impl->pidMutex);
    if (m_impl->pid > 0) {
        ::kill(m_impl->tree ? -m_impl->pid : m_impl->pid, SIGKILL);
    }
}

#endif

ChildProcess::ChildProcess(std::unique_ptr<Impl> impl) : m_impl(std::move(impl)) {}

ChildProcess::~ChildProcess() {
    wait();
}

}  // namespace black::core
