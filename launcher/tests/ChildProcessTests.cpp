#include "core/ChildProcess.h"

#include <doctest/doctest.h>

#include <cstdlib>
#include <mutex>
#include <vector>

using black::core::ChildOptions;
using black::core::ChildProcess;

namespace {

struct Captured {
    std::mutex mutex;
    std::vector<std::string> out;
    std::vector<std::string> err;
};

/// A shell running `script`: cmd.exe on Windows, sh elsewhere.
ChildOptions shell(const std::string& script, Captured& captured) {
    ChildOptions options;
#ifdef _WIN32
    options.program = "cmd.exe";
    options.arguments = {"/d", "/c", script};
#else
    options.program = "/bin/sh";
    options.arguments = {"-c", script};
#endif
    options.onLine = [&captured](std::string_view line, bool isError) {
        const std::scoped_lock lock(captured.mutex);
        (isError ? captured.err : captured.out).emplace_back(line);
    };
    return options;
}

}  // namespace

TEST_CASE("A child's output arrives line by line, and its exit code is returned") {
    Captured captured;
#ifdef _WIN32
    auto options = shell("echo one& echo two& echo oops 1>&2& exit 3", captured);
#else
    auto options = shell("echo one; echo two; echo oops >&2; exit 3", captured);
#endif
    auto child = ChildProcess::start(std::move(options));
    REQUIRE(child.ok());
    CHECK(child.value()->wait() == 3);
    CHECK(captured.out == std::vector<std::string>{"one", "two"});
    REQUIRE(captured.err.size() == 1);
    CHECK(captured.err[0].starts_with("oops"));
}

TEST_CASE("A child gets the given variables, without the dropped inherited ones") {
    Captured captured;
#ifdef _WIN32
    _putenv_s("RECOMP_LAUNCHER_TEST_INHERITED", "leaked");
    auto options = shell("echo [%RECOMP_LAUNCHER_TEST_INHERITED%] [%BLACK_TEST_VALUE%]", captured);
#else
    setenv("RECOMP_LAUNCHER_TEST_INHERITED", "leaked", 1);  // NOLINT(concurrency-mt-unsafe)
    auto options = shell("echo [$RECOMP_LAUNCHER_TEST_INHERITED] [$BLACK_TEST_VALUE]", captured);
#endif
    options.dropPrefixes = {"RECOMP_"};
    options.environment = {{"BLACK_TEST_VALUE", "set"}};
    auto child = ChildProcess::start(std::move(options));
    REQUIRE(child.ok());
    CHECK(child.value()->wait() == 0);
    REQUIRE(captured.out.size() == 1);
#ifdef _WIN32
    CHECK(captured.out[0] == "[%RECOMP_LAUNCHER_TEST_INHERITED%] [set]");  // cmd leaves unknown names as written
#else
    CHECK(captured.out[0] == "[] [set]");
#endif
}

TEST_CASE("kill ends a child that would run on") {
    Captured captured;
#ifdef _WIN32
    auto options = shell("ping -n 30 127.0.0.1 >NUL", captured);
#else
    auto options = shell("sleep 30", captured);
#endif
    auto child = ChildProcess::start(std::move(options));
    REQUIRE(child.ok());
    child.value()->kill();
    CHECK(child.value()->wait() != 0);
}

TEST_CASE("A missing program is an error") {
    ChildOptions options;
    options.program = "black-launcher-no-such-program-here";
    auto child = ChildProcess::start(std::move(options));
#ifdef _WIN32
    CHECK_FALSE(child.ok());
#else
    // exec fails in the forked child, which exits 127
    REQUIRE(child.ok());
    CHECK(child.value()->wait() == 127);
#endif
}
