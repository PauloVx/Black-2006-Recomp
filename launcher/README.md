# BLACK Launcher

The player-facing launcher, in C++20 on [Walnut](https://github.com/StudioCherno/Walnut) (Vulkan + Dear ImGui).
It replaced the C# launcher; see [the plan](../docs/plans/new-launcher.md) for what is still to come (building the
game from the launcher).

It sets up the disc: an ISO (XISO or a full Redump-style dump) or a folder of extracted files, chosen with the
system's file dialog, dropped onto the window or given on the command line. The launcher reads Xbox disc images
itself (`src/core/Xdvdfs`), checks `default.xbe` before copying, and copies into `game/` with progress, resume and
cancel. It plays the game (`src/core/GameSession`: the `RECOMP_*` environment, the menu-language bank, and a
`reports/launcher-*` session with capped diagnostic logs), edits every setting the game reads, edits save profiles
(`src/core/Profile`: checksums and the HMAC-SHA1 signature, backups first) and prepares problem reports.

It builds the game (`src/core/Toolchain`: finds Visual Studio's C++ tools, CMake, Python and Rust, also where
their installers put them, and installs missing ones with winget; `src/core/GameBuild`: the stages, with progress
read from `generate.py`, MSBuild and Ninja, a log in `reports/build-*.log`, and stopping that ends the whole
process tree). On Linux the build runs `scripts/setup-linux.sh --game-only`, and missing tools are listed with the
command that installs them.

`BLACK Launcher --play` starts the game with the saved settings and no window, and waits for it: the shortcuts
(`Play BLACK.sh`) use it. `BLACK Launcher --build [--from-scratch]` builds the game in the terminal;
`scripts/setup.ps1` uses it.

## Building

Needs CMake 3.21+ and a C++20 compiler; nothing else. The Vulkan headers are a submodule
(`vendor/vulkan-headers`), and the launcher links against the Vulkan loader that graphics drivers install: on
Windows through an import library made at configure time. `-DBLACK_LAUNCHER_VULKAN_SDK=ON` builds against an
installed SDK instead.

```powershell
git submodule update --init --recursive
cd launcher
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

On Linux, use GCC 13 or newer with C++20 `std::format` support (Ubuntu 24.04:
`sudo apt install build-essential cmake ninja-build libx11-dev libxrandr-dev libxinerama-dev
libxcursor-dev libxi-dev libwayland-dev wayland-protocols libxkbcommon-dev`, plus a graphics driver with Vulkan),
use the `linux-release` presets. The launcher is written to `build-launcher*/bin/` and finds the project folder
by searching upwards from itself, so it runs from the build folder too. `scripts/build-launcher.ps1` and
`scripts/setup-linux.sh` build it and copy it to the project folder.

## Layout

```
launcher/
  src/core/   the logic, with no UI: project layout, disc import (XDVDFS), settings and key bindings, playing
              the game, save profiles, problem reports, the game's menu fonts. Unit-tested.
  src/app/    the Walnut application: the pages (disc, settings, saves), the game-style widgets, key capture,
              translations, the theme
  tests/      doctest unit tests for src/core; no game files, GPU or window needed
  assets/     the logo, stand-in fonts (SIL Open Font License) and translations, built into the executable
  vendor/     submodules: walnut, imgui, glfw, glm, spdlog, json, vulkan-headers, doctest
```

## The look

The launcher is drawn like the game's own menu pages: the colours are sampled from them, rows are green-grey with
the selected one bright and its selector line, the rails run down the left, and prompts sit along the bottom. The
banner shows BLACK's logo (`assets/images/black-logo.png`, built in; a `local/logo.png` replaces it). Once the
player's disc files are in place, text uses the game's own `Small`/`Big` menu fonts, read from `language/fonts` at
run time and never shipped. Until then the closest free fonts stand in: Russo One for rows and help, Saira
Condensed ExtraBold for titles.

## Translations

All text a player sees goes through `tr()` / `trf()` (`src/app/Translations.h`), looked up by its English in
`assets/translations/<language>.json`. `python scripts/check-launcher-translations.py` lists text with no
translation (CI runs it); `--update` adds new text to every catalog for translators to fill in. The game's fonts
have no A- or O-tilde, so `MenuFont` draws those with the tilde of the font's own N-tilde.

## Walnut

`vendor/walnut` is [PauloVx/Walnut](https://github.com/PauloVx/Walnut), branch `black`: Walnut's `dev` branch, which
has the custom titlebar, plus small changes for embedding it. These include no submodules of its own, opt-in file
logs, application fonts, titlebar colour and icon, and keeping the system frame on Linux where TheCherno's GLFW fork
has no custom titlebar. Change Walnut there, then commit the new submodule commit here.

## Code style

- C++20, warnings at `/W4` / `-Wall -Wextra -Wpedantic -Wconversion` and more. CI treats them as errors
  (`*-ci` presets).
- `launcher/.clang-format` and `launcher/.clang-tidy` apply to `src/` and `tests/`. Run
  `python scripts/check-launcher.py --fix` before committing. `--tidy <build folder>` runs clang-tidy on a Ninja
  build (the Linux presets).
- Logic goes in `src/core` with tests. `src/app` only draws and wires actions. Failures that a player can hit
  return `core::Result<T>` with a message they can read. Resources are owned by RAII types, with no raw `new`
  except where Walnut's API needs it.
