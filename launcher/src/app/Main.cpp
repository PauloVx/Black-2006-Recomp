#include "app/EmbeddedAssets.h"
#include "app/LauncherLayer.h"
#include "app/NativeDialogs.h"
#include "app/Theme.h"
#include "app/Translations.h"
#include "core/GameBuild.h"
#include "core/GameSession.h"
#include "core/Text.h"

#include "Walnut/Application.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
// after windows.h
#include <shellapi.h>
#endif

// Walnut's Application clears this when closed; its EntryPoint.h would define it, but the launcher has its own main()
bool g_ApplicationRunning = true;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

namespace {

Walnut::FontSpecification embeddedFont(const char* name, std::span<const unsigned char> data, float sizePixels) {
    Walnut::FontSpecification font;
    font.Name = name;
    font.Data = data.data();
    font.DataSize = data.size();
    font.SizePixels = sizePixels;
    return font;
}

/// The arguments as UTF-8 (argv is in the ANSI code page on Windows; the wide command line keeps any path intact).
std::vector<std::string> arguments(int argc, char** argv) {
    std::vector<std::string> result;
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int count = 0;
    wchar_t** wide = CommandLineToArgvW(GetCommandLineW(), &count);
    for (int i = 0; wide != nullptr && i < count; ++i) {
        result.push_back(black::core::toUtf8(std::filesystem::path(wide[i])));  // NOLINT
    }
    LocalFree(wide);
#else
    for (int i = 0; i < argc; ++i) {
        result.emplace_back(argv[i]);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
#endif
    return result;
}

std::filesystem::path searchStart(const std::vector<std::string>& args) {
    if (!args.empty()) {
        std::error_code error;
        const std::filesystem::path executable = std::filesystem::absolute(black::core::pathFromUtf8(args[0]), error);
        if (!error && black::core::ProjectLayout::locate(executable.parent_path())) {
            return executable.parent_path();
        }
    }
    return std::filesystem::current_path();
}

/// --play: starts the game with the saved settings and no launcher window, and waits for it (the logs need the
/// launcher until the game ends). Returns the game's exit code.
int playWithoutWindow(const std::filesystem::path& start) {
    const auto project = black::core::ProjectLayout::locate(start);
    if (!project) {
        black::app::showError("BLACK", "The launcher is not inside the BLACK project folder.");
        return 1;
    }
    const black::core::Settings settings = black::core::loadSettings(*project);
    black::app::setLanguage(settings.menuLanguage);
    auto session = black::core::GameSession::start(*project, settings);
    if (!session) {
        black::app::showError("BLACK", session.error().message);
        return 1;
    }
    const int code = session.value()->wait();
    if (const std::string failure = session.value()->logFailure(); !failure.empty()) {
        black::app::showError("BLACK", black::app::trf("The game's log could not be written: {0}", {failure}));
    }
    return code;
}

void say(const std::string& line) {
    std::fwrite(line.data(), 1, line.size(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

/// --build [--from-scratch]: builds the game in the terminal, as the launcher's Build page does (the setup scripts
/// use it). Missing tools are listed, not installed. Returns 0 once the game is built.
int buildWithoutWindow(const std::filesystem::path& start, bool fromScratch) {
    const auto project = black::core::ProjectLayout::locate(start);
    if (!project) {
        say("The launcher is not inside the BLACK project folder.");
        return 1;
    }
    const black::core::Toolchain toolchain = black::core::detectToolchain(*project);
    for (const black::core::Tool& tool : toolchain.tools) {
        say((tool.found      ? "  found    "
             : tool.required ? "  MISSING  "
                             : "  -        ") +
            tool.name + (tool.version.empty() ? std::string() : "  " + tool.version));
    }
    if (!toolchain.ready()) {
        say("Install the missing tools (BLACK Launcher > Build the game does it with winget), then try again.");
        return 1;
    }
    std::string printed;
    auto stage = static_cast<black::core::BuildStage>(-1);
    const auto outcome = black::core::buildGame(*project, toolchain, black::core::BuildOptions{fromScratch}, {},
                                                [&](const black::core::BuildProgress& progress) {
                                                    if (progress.stage != stage) {
                                                        stage = progress.stage;
                                                        say("== Stage " + std::to_string(static_cast<int>(stage) + 1) +
                                                            " of " + std::to_string(black::core::kBuildStages));
                                                    }
                                                    if (progress.activity != printed) {
                                                        printed = progress.activity;
                                                        say(printed);
                                                    }
                                                });
    say(outcome.ok ? "The game is built." : outcome.message);
    if (!outcome.warning.empty()) {
        say(outcome.warning);
    }
    say("Log: " + black::core::toUtf8(outcome.log));
    return outcome.ok ? 0 : 1;
}

Walnut::Application* createApplication(const std::vector<std::string>& args) {
    using black::app::LauncherLayer;

    Walnut::ApplicationSpecification specification;
    specification.Name = "BLACK";
    specification.Width = 960;
    specification.Height = 640;
    specification.WindowResizeable = false;
    specification.CenterWindow = true;
    specification.CustomTitlebar = true;  // Windows; elsewhere Walnut keeps the system frame
    specification.TitlebarShowsName = false;
    specification.TitlebarShowsIcon = false;
    specification.TitlebarColor = black::app::toU32(black::app::colors::kTitlebar);
    specification.UseDockspace = true;  // hosts the custom titlebar; the launcher draws one window below it
    specification.UseViewports = false;
    specification.Fonts = {
        embeddedFont(LauncherLayer::kRowFont, black::assets::rowFont(), 26.0f),
        embeddedFont(LauncherLayer::kTitleFont, black::assets::titleFont(), 44.0f),
    };

    auto* application = new Walnut::Application(specification);  // NOLINT(cppcoreguidelines-owning-memory)
    std::optional<std::filesystem::path> disc;
    if (args.size() > 1) {
        disc = black::core::pathFromUtf8(args[1]);  // an ISO or folder dropped on the launcher's icon
    }
    application->PushLayer(std::make_shared<LauncherLayer>(searchStart(args), std::move(disc)));
    return application;
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args = arguments(argc, argv);
    if (args.size() == 2 && args[1] == "--play") {
        return playWithoutWindow(searchStart(args));
    }
    if (args.size() >= 2 && args[1] == "--build") {
        return buildWithoutWindow(searchStart(args), args.size() == 3 && args[2] == "--from-scratch");
    }
    const std::unique_ptr<Walnut::Application> application(createApplication(args));
    application->Run();
    return 0;
}
