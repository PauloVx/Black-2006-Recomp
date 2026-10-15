#include "app/EmbeddedAssets.h"
#include "app/LauncherLayer.h"
#include "app/Theme.h"

#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"  // defines main()/WinMain: include in this file only

#include <filesystem>
#include <memory>

namespace {

Walnut::FontSpecification embeddedFont(const char* name, std::span<const unsigned char> data, float sizePixels) {
    Walnut::FontSpecification font;
    font.Name = name;
    font.Data = data.data();
    font.DataSize = data.size();
    font.SizePixels = sizePixels;
    return font;
}

/// The launcher sits in the project folder; when started from elsewhere (an IDE, a shortcut) it also searches upwards
/// from the current folder.
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

}  // namespace

Walnut::Application* Walnut::CreateApplication(int argc, char** argv) {
    (void)argc;
    using black::app::LauncherLayer;

    Walnut::ApplicationSpecification specification;
    specification.Name = "BLACK";
    specification.Width = 960;
    specification.Height = 640;
    specification.WindowResizeable = false;
    specification.CenterWindow = true;
    specification.CustomTitlebar = true;  // Windows; elsewhere Walnut keeps the system frame
    specification.TitlebarShowsName = false;
    specification.TitlebarColor = black::app::toU32(black::app::colors::kTitlebar);
    specification.UseDockspace = true;  // hosts the custom titlebar; the launcher draws one window below it
    specification.UseViewports = false;
    // Atlas sizes: the stencil is drawn large in the banner, the condensed faces stand in for the game's fonts.
    specification.Fonts = {
        embeddedFont(LauncherLayer::kStencilFont, black::assets::stencilFont(), 96.0f),
        embeddedFont(LauncherLayer::kCondensedFont, black::assets::condensedFont(), 26.0f),
        embeddedFont(LauncherLayer::kCondensedBoldFont, black::assets::condensedBoldFont(), 44.0f),
    };

    // Walnut's entry point owns and deletes the application.
    auto* application = new Walnut::Application(specification);  // NOLINT(cppcoreguidelines-owning-memory)
    application->PushLayer(std::make_shared<LauncherLayer>(searchStart(argv)));
    return application;
}
