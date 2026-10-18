#include "core/Toolchain.h"

#include "core/ChildProcess.h"
#include "core/Text.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace black::core {
namespace fs = std::filesystem;
namespace {

std::string variable(const char* name) {
#ifdef _WIN32
    const std::wstring wide = pathFromUtf8(name).wstring();
    const DWORD size = GetEnvironmentVariableW(wide.c_str(), nullptr, 0);
    if (size == 0) {
        return {};
    }
    std::wstring value(size, L'\0');
    value.resize(GetEnvironmentVariableW(wide.c_str(), value.data(), size));
    return toUtf8(fs::path(value));
#else
    const char* value = std::getenv(name);  // NOLINT(concurrency-mt-unsafe): read on one thread
    return value == nullptr ? std::string() : value;
#endif
}

bool isFile(const fs::path& path) {
    std::error_code error;
    return fs::is_regular_file(path, error);
}

#ifdef _WIN32
std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> result;
    std::istringstream stream(text);
    for (std::string line; std::getline(stream, line);) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            result.push_back(line);
        }
    }
    return result;
}
#endif

#ifdef _WIN32
constexpr char kPathSeparator = ';';

/// PATH as the system has it now (the user's and the machine's), which installers update.
std::string registryPath() {
    std::string result;
    const auto read = [&result](HKEY root, const wchar_t* key) {
        // REG_SZ alone: a REG_EXPAND_SZ value comes back with its variables expanded
        DWORD size = 0;
        if (RegGetValueW(root, key, L"Path", RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS || size == 0) {
            return;
        }
        std::wstring value(size / sizeof(wchar_t), L'\0');
        if (RegGetValueW(root, key, L"Path", RRF_RT_REG_SZ, nullptr, value.data(), &size) != ERROR_SUCCESS) {
            return;
        }
        value.resize(wcsnlen(value.c_str(), value.size()));
        result += (result.empty() ? "" : ";") + toUtf8(fs::path(value));
    };
    read(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment");
    read(HKEY_CURRENT_USER, L"Environment");
    return result;
}
#else
constexpr char kPathSeparator = ':';
#endif

/// Folders on PATH (the system's current one first on Windows).
std::vector<fs::path> searchPath() {
    std::string path = variable("PATH");
#ifdef _WIN32
    path = registryPath() + ";" + path;
#endif
    std::vector<fs::path> folders;
    std::istringstream stream(path);
    for (std::string folder; std::getline(stream, folder, kPathSeparator);) {
        if (!folder.empty()) {
            folders.push_back(pathFromUtf8(folder));
        }
    }
    return folders;
}

/// `name` in the PATH folders and then the given ones, skipping Windows' Store placeholders.
std::vector<fs::path> candidates(std::string_view name, const std::vector<fs::path>& extra) {
    std::vector<fs::path> found;
    const auto consider = [&found](const fs::path& path) {
        if (isFile(path) && toUtf8(path).find("WindowsApps") == std::string::npos &&
            std::find(found.begin(), found.end(), path) == found.end()) {
            found.push_back(path);
        }
    };
    for (const fs::path& folder : searchPath()) {
#ifdef _WIN32
        consider(folder / (std::string(name) + ".exe"));
#else
        consider(folder / name);
#endif
    }
    for (const fs::path& path : extra) {
        consider(path);
    }
    return found;
}

#ifdef _WIN32
/// Each folder matching `prefix*` in `parent`, newest name first ("Python312" before "Python310").
std::vector<fs::path> versionedFolders(const fs::path& parent, std::string_view prefix) {
    std::vector<fs::path> found;
    std::error_code error;
    for (fs::directory_iterator it(parent, error), end; !error && it != end; it.increment(error)) {
        if (it->is_directory(error) && toUtf8(it->path().filename()).starts_with(prefix)) {
            found.push_back(it->path());
        }
    }
    std::sort(found.rbegin(), found.rend());
    return found;
}
#endif

void addFolder(Toolchain& toolchain, const fs::path& program) {
    const fs::path folder = program.parent_path();
    if (!folder.empty() &&
        std::find(toolchain.folders.begin(), toolchain.folders.end(), folder) == toolchain.folders.end()) {
        toolchain.folders.push_back(folder);
    }
}

/// The first candidate whose version passes `accept`, recorded in `tool`.
void pick(Tool& tool, const std::vector<fs::path>& programs, const std::vector<std::string>& arguments, int major,
          int minor) {
    for (const fs::path& program : programs) {
        const auto output = captureOutput(program, arguments);
        const auto version = output ? parseVersion(*output) : std::nullopt;
        if (version && atLeast(*version, major, minor)) {
            tool.found = true;
            tool.program = program;
            tool.version = std::to_string((*version)[0]) + "." + std::to_string((*version)[1]) + "." +
                           std::to_string((*version)[2]);
            return;
        }
    }
}

#ifdef _WIN32
void detectWindows(Toolchain& toolchain, const ProjectLayout& project) {
    const fs::path programFiles = pathFromUtf8(variable("ProgramFiles"));
    const fs::path programFilesX86 = pathFromUtf8(variable("ProgramFiles(x86)"));
    const fs::path localAppData = pathFromUtf8(variable("LOCALAPPDATA"));
    const fs::path home = pathFromUtf8(variable("USERPROFILE"));

    Tool cpp{"cpp", "VISUAL STUDIO 2022 C++ TOOLS", "Compiles the game (the Build Tools are enough)."};
    cpp.package = "Microsoft.VisualStudio.2022.BuildTools";
    cpp.installArguments = {"--override",
                            "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools "
                            "--includeRecommended"};
    fs::path visualStudio;
    const fs::path vswhere = programFilesX86 / "Microsoft Visual Studio" / "Installer" / "vswhere.exe";
    if (isFile(vswhere)) {
        const std::vector<std::string> query = {"-version",  "[17.0,18.0)",
                                                "-products", "*",
                                                "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                                "-latest"};
        auto withProperty = [&](const char* property) {
            std::vector<std::string> arguments = query;
            arguments.insert(arguments.end(), {"-property", property});
            const auto output = captureOutput(vswhere, arguments);
            const auto found = output ? lines(*output) : std::vector<std::string>{};
            return found.empty() ? std::string() : found.front();
        };
        if (const std::string path = withProperty("installationPath"); !path.empty()) {
            visualStudio = pathFromUtf8(path);
            cpp.found = true;
            cpp.program = visualStudio;
            cpp.version = withProperty("installationVersion");
        }
    }
    toolchain.tools.push_back(cpp);

    Tool cmake{"cmake", "CMAKE", "Sets up the game's build."};
    cmake.package = "Kitware.CMake";
    cmake.installArguments = {"--silent"};
    std::vector<fs::path> cmakeExtra = {programFiles / "CMake" / "bin" / "cmake.exe"};
    if (!visualStudio.empty()) {
        cmakeExtra.push_back(visualStudio / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "CMake" /
                             "bin" / "cmake.exe");
    }
    pick(cmake, candidates("cmake", cmakeExtra), {"--version"}, 3, 21);
    toolchain.tools.push_back(cmake);

    Tool python{"python", "PYTHON", "Translates the game's code from your disc (3.10 or newer)."};
    python.package = "Python.Python.3.12";
    python.installArguments = {"--silent"};
    std::vector<fs::path> pythonExtra;
    for (const fs::path& parent : {localAppData / "Programs" / "Python", programFiles}) {
        for (const fs::path& folder : versionedFolders(parent, "Python3")) {
            pythonExtra.push_back(folder / "python.exe");
        }
    }
    pick(python, candidates("python", pythonExtra), {"--version"}, 3, 10);
    toolchain.tools.push_back(python);

    Tool rust{"rust", "RUST", "Builds the game's audio processor. It fetches the exact version it needs itself."};
    rust.package = "Rustlang.Rustup";
    rust.installArguments = {"--silent"};
    pick(rust, candidates("cargo", {home / ".cargo" / "bin" / "cargo.exe"}), {"--version"}, 1, 0);
    toolchain.tools.push_back(rust);

    // Git only fetches a clone's missing submodules; a release archive has them
    std::error_code error;
    const bool needGit = fs::exists(project.root() / ".git", error) &&
                         !isFile(project.root() / "third_party" / "xboxrecomp" / "CMakeLists.txt");
    Tool git{"git", "GIT", "Fetches the parts of the project a clone is missing.", needGit};
    git.package = "Git.Git";
    git.installArguments = {"--silent"};
    pick(git, candidates("git", {programFiles / "Git" / "cmd" / "git.exe"}), {"--version"}, 2, 0);
    toolchain.tools.push_back(git);

    toolchain.canInstall = captureOutput("winget", {"--version"}).has_value();
}
#else
void detectLinux(Toolchain& toolchain) {
    const std::vector<fs::path> llvm = {"/usr/lib/llvm-18/bin"};
    const auto all = [&](Tool& tool, const std::vector<std::string>& programs) {
        tool.found = true;
        for (const std::string& name : programs) {
            std::vector<fs::path> extra;
            for (const fs::path& folder : llvm) {
                extra.push_back(folder / name);
            }
            const auto found = candidates(name, extra);
            if (found.empty()) {
                tool.found = false;
                return;
            }
            tool.program = found.front();
        }
    };
    Tool clang{"clang", "CLANG AND LLVM 18", "Compiles the game for Windows, on Linux."};
    all(clang, {"clang-cl", "lld-link", "llvm-lib", "llvm-rc", "llvm-mt"});
    toolchain.tools.push_back(clang);
    Tool cpp{"cpp", "C AND C++ COMPILERS", "Build the launcher and the game's build tools."};
    all(cpp, {"cc", "c++"});
    toolchain.tools.push_back(cpp);
    Tool cmake{"cmake", "CMAKE AND NINJA", "Set up and run the game's build."};
    all(cmake, {"cmake", "ninja"});
    toolchain.tools.push_back(cmake);
    Tool python{"python", "PYTHON", "Translates the game's code from your disc."};
    pick(python, candidates("python3", {}), {"--version"}, 3, 10);
    toolchain.tools.push_back(python);
    Tool git{"git", "GIT", "Fetches the build's pinned tools and missing project parts."};
    pick(git, candidates("git", {}), {"--version"}, 2, 0);
    toolchain.tools.push_back(git);
    Tool wine{"wine", "WINE", "Runs the game once it is built (or set BLACK_PROTON for Proton).", false};
    all(wine, {"wine"});
    toolchain.tools.push_back(wine);
    toolchain.canInstall = false;
}
#endif

}  // namespace

std::optional<std::array<int, 3>> parseVersion(std::string_view text) {
    static const std::regex pattern(R"((\d+)\.(\d+)(?:\.(\d+))?)");
    std::match_results<std::string_view::const_iterator> match;
    if (!std::regex_search(text.begin(), text.end(), match, pattern)) {
        return std::nullopt;
    }
    return std::array<int, 3>{std::stoi(match[1].str()), std::stoi(match[2].str()),
                              match[3].matched ? std::stoi(match[3].str()) : 0};
}

bool atLeast(const std::array<int, 3>& version, int major, int minor) {
    return version[0] > major || (version[0] == major && version[1] >= minor);
}

std::optional<std::string> captureOutput(const fs::path& program, const std::vector<std::string>& arguments) {
    auto output = std::make_shared<std::string>();
    auto mutex = std::make_shared<std::mutex>();
    ChildOptions options;
    options.program = program;
    options.arguments = arguments;
    options.onLine = [output, mutex](std::string_view line, bool) {
        const std::scoped_lock lock(*mutex);
        output->append(line).push_back('\n');
    };
    auto child = ChildProcess::start(std::move(options));
    if (!child || child.value()->wait() != 0) {
        return std::nullopt;
    }
    return *output;
}

bool Toolchain::ready() const {
    return std::all_of(tools.begin(), tools.end(), [](const Tool& tool) { return tool.found || !tool.required; });
}

const Tool* Toolchain::find(std::string_view key) const {
    const auto it = std::find_if(tools.begin(), tools.end(), [key](const Tool& tool) { return tool.key == key; });
    return it == tools.end() ? nullptr : &*it;
}

std::string Toolchain::pathVariable() const {
    std::string path;
    for (const fs::path& folder : folders) {
        path += toUtf8(folder) + kPathSeparator;
    }
#ifdef _WIN32
    path += registryPath() + kPathSeparator;
#endif
    return path + variable("PATH");
}

Toolchain detectToolchain(const ProjectLayout& project) {
    Toolchain toolchain;
#ifdef _WIN32
    detectWindows(toolchain, project);
#else
    (void)project;
    detectLinux(toolchain);
#endif
    for (const Tool& tool : toolchain.tools) {
        if (tool.found && tool.key != "cpp") {
            addFolder(toolchain, tool.program);
        }
    }
    return toolchain;
}

std::vector<InstallCommand> installCommands(const Toolchain& toolchain) {
    std::vector<InstallCommand> commands;
    if (!toolchain.canInstall) {
        return commands;
    }
    for (const Tool& tool : toolchain.tools) {
        if (tool.found || !tool.required || tool.package.empty()) {
            continue;
        }
        InstallCommand command{
            tool.name,
            "winget",
            {"install", "--id", tool.package, "--exact", "--source", "winget", "--accept-package-agreements",
             "--accept-source-agreements", "--disable-interactivity"}};
        command.arguments.insert(command.arguments.end(), tool.installArguments.begin(), tool.installArguments.end());
        commands.push_back(std::move(command));
    }
    return commands;
}

std::string installHint(const Toolchain& toolchain) {
    if (toolchain.ready()) {
        return {};
    }
    return "sudo apt-get install -y build-essential clang-18 clang-tools-18 llvm-18 lld-18 python3-venv cmake "
           "ninja-build git ca-certificates wine64";
}

}  // namespace black::core
