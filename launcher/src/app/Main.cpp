#include "app/EmbeddedAssets.h"
#include "app/LauncherLayer.h"
#include "app/Theme.h"
#include "core/Text.h"

#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"  // defines main()/WinMain: include in this file only

#include <filesystem>
#include <memory>
#include <optional>

#ifdef _WIN32
#include <windows.h>
// after windows.h
#include <shellapi.h>
#endif

namespace {

Walnut::FontSpecification embeddedFont(const char* name, std::span<const unsigned char> data, float sizePixels) {
    Walnut::FontSpecification font;
    font.Name = name;
    font.Data = data.data();
    font.DataSize = data.size();
    font.SizePixels = sizePixels;
    return font;
}

std::filesystem::path searchStart(char** argv) {
    if (argv != nullptr && argv[0] != nullptr) {
        std::error_code error;
        const std::filesystem::path executable = std::filesystem::absolute(argv[0], error);
        if (!error && black::core::ProjectLayout::locate(executable.parent_path())) {
            return executable.parent_path();
        }
    }
    return std::filesystem::current_path();
}

std::optional<std::filesystem::path> discArgument(int argc, char** argv) {
#ifdef _WIN32
    // argv is in the ANSI code page on Windows; the wide command line keeps any path intact
    (void)argv;
    int count = 0;
    wchar_t** wide = CommandLineToArgvW(GetCommandLineW(), &count);
    std::optional<std::filesystem::path> disc;
    if (wide != nullptr && count > 1) {
        disc = std::filesystem::path(wide[1]);
    }
    LocalFree(wide);
    (void)argc;
    return disc;
#else
    if (argc > 1 && argv[1] != nullptr) {
        return black::core::pathFromUtf8(argv[1]);
    }
    return std::nullopt;
#endif
}

}  // namespace

Walnut::Application* Walnut::CreateApplication(int argc, char** argv) {
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

    // Walnut's entry point owns and deletes the application.
    auto* application = new Walnut::Application(specification);  // NOLINT(cppcoreguidelines-owning-memory)
    std::optional<std::filesystem::path> disc = discArgument(argc, argv);
    application->PushLayer(std::make_shared<LauncherLayer>(searchStart(argv), std::move(disc)));
    return application;
}
