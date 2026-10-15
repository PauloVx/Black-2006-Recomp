# BLACK Launcher

The player-facing launcher, in C++20 on [Walnut](https://github.com/StudioCherno/Walnut) (Vulkan + Dear ImGui).
It will replace the C# launcher; see [the plan](../docs/plans/new-launcher.md).

It currently shows where a copy stands (disc, build) and offers Play, Settings, Open game folder and Quit. Play and
Settings start the existing `BLACK PC Launcher` / `BLACK PC Settings` until the settings pages and the setup wizard
(ISO, build tools, build) move here.

## Building

Needs CMake 3.21+, a C++20 compiler and the Vulkan headers and loader: the
[Vulkan SDK](https://vulkan.lunarg.com/) on Windows, `libvulkan-dev` on Linux. Players only need a graphics driver
with Vulkan.

```powershell
git submodule update --init --recursive
cd launcher
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

On Linux (Ubuntu: `sudo apt install build-essential cmake ninja-build libvulkan-dev libx11-dev libxrandr-dev
libxinerama-dev libxcursor-dev libxi-dev libwayland-dev wayland-protocols libxkbcommon-dev`), use the `linux-release`
presets. The launcher is written to `build-launcher*/bin/` and finds the project folder by searching upwards from
itself, so it runs from the build folder too.

## Layout

```
launcher/
  src/core/   the logic, with no UI: project layout, disc check (SHA-256), the game's fonts and logo (DXT,
              "4.1v" fonts, APT bundles), processes. Unit-tested.
  src/app/    the Walnut application: the screen, the game-style widgets, the theme
  tests/      doctest unit tests for src/core; no game files, GPU or window needed
  assets/     fonts built into the executable (SIL Open Font License)
  vendor/     submodules: walnut, imgui, glfw, glm, spdlog, doctest
```

## The look

The launcher is drawn like the game's own menu pages: the colours are sampled from them, rows are green-grey with
the selected one bright and its selector line, the rails run down the left, and prompts sit along the bottom. Once
the player's disc files are in place it uses the game's own assets, read from those files at run time and never
shipped: the logo from `export/FrontEnd/Core.bin` and the `Small`/`Big` menu fonts from `language/fonts`. Until then
it uses Black Ops One (a stencil close to the logo) and Saira Condensed.

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
