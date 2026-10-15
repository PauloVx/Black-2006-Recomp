#pragma once

#include "core/Result.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace black::core {

struct ChildOptions {
    std::filesystem::path program;
    std::vector<std::string> arguments;
    std::filesystem::path workingDirectory;
    /// Inherited variables starting with one of these are dropped before `environment` is added.
    std::vector<std::string> dropPrefixes;
    std::map<std::string, std::string> environment;
    /// Called from reader threads with each line of output, without its line break.
    std::function<void(std::string_view line, bool isError)> onLine;
    /// The program and everything it starts are one tree: kill() ends all of it, and what is left running when
    /// the program exits (a compiler's helper, say) is ended then too.
    bool killTree = false;
};

/// A program the launcher starts and watches: its output arrives line by line, and it can be waited for or ended.
class ChildProcess {
public:
    [[nodiscard]] static Result<std::unique_ptr<ChildProcess>> start(ChildOptions options);

    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;
    ChildProcess(ChildProcess&&) = delete;
    ChildProcess& operator=(ChildProcess&&) = delete;
    /// Waits for the program; end it first with kill() to not wait.
    ~ChildProcess();

    [[nodiscard]] int pid() const noexcept;
    /// Blocks until the program exits and all its output has been delivered. Returns the exit code.
    int wait();
    void kill();

    struct Impl;

private:
    explicit ChildProcess(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> m_impl;
};

}  // namespace black::core
