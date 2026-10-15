# Plan: a new C++ launcher (Walnut) and XboxRecomp as a submodule

Decisions so far:
- The launcher and all new hand-written code are C++20. Existing hand-written C stays C. Generated code is untouched.
- The launcher installs missing build tools itself, with one click.
- XboxRecomp becomes a fork plus a submodule, and the patch file goes away.
- Walnut is ported so the launcher also builds natively on Linux.

## What a player does

1. Download the project (a release zip, or a clone) and run **BLACK Launcher**.
2. **Choose your BLACK disc**: an ISO file (Redump or XISO) or a folder of extracted files.
   The launcher reads the ISO directly. It finds the game partition, extracts it to `game\` and
   checks `default.xbe` against the supported retail hash. It tells you plainly if the disc
   is a different version.
3. **Install what's needed**: a checklist of the build tools (VS Build Tools C++, CMake,
   Git, Python, Rust), each marked found or missing. One **Install** button gets the missing
   ones, with a progress bar. It asks for administrator rights once.
4. **Build**: one button with a progress bar and a "show details" log. It sets up the
   runtime, generates the code from your XBE and compiles. It can resume after a failure,
   and if something goes wrong it shows the step that failed with a plain explanation.
5. **Play**. Later launches open straight to Play, with Settings next to it.

Settings keep everything the old launcher had: display, graphics (including Reflections),
input and key bindings, language, save/profile editing and diagnostics. They use the same
`local\video.ini` / `local\controls.ini` files the game reads.

## What a developer gets

- One CMake project for everything. `cmake --preset windows-release` (or `linux-release`)
  builds the launcher. The launcher's Build step uses the same presets.
- Submodules instead of clone-and-patch:
  - `third_party/xboxrecomp`: PauloVx's fork of sp00nznet/xboxrecomp. Today's patch becomes
    real commits on a `black` branch, so runtime edits are ordinary commits and PRs.
  - `third_party/dsp56300`: pinned v1.0.0 (today it is a clone in `.work/`).
  - `launcher/vendor/walnut`: Walnut (fork only if the Linux port needs changes to it) and its
    own submodules: imgui (docking), glfw, glm, stb.
- Launcher code layout:
  ```
  launcher/
    CMakeLists.txt
    src/app/        Walnut application, layers, screens (UI only)
    src/core/       no UI: XISO reader, XBE check, tool detection/install, build runner,
                    ini models (video/controls/launcher), save-profile model, localisation
    src/platform/   Windows / Linux specifics behind small interfaces (process spawning,
                    elevation, package install, paths)
    tests/          doctest unit tests for src/core (no game files needed)
  ```
- Coding standard for hand-written C++: clang-format and clang-tidy configs committed, and
  warnings as errors in CI. RAII and `std::filesystem`, no raw `new`/`delete`. `std::expected`-style
  results for fallible steps. UI and logic are kept apart so `core/` can be tested.
- CI (GitHub Actions): build plus unit tests on Windows and Linux, and the existing runtime
  test suites.

## Phases

1. **Submodules.** Fork xboxrecomp (you click Fork on GitHub, or install the `gh` CLI and
   log in). Turn the patch into commits on the fork. Add the submodules. Point `setup` and
   `Build BLACK.*` at them and remove `patches/xboxrecomp.patch`. Update CI and docs.
   Done when a fresh `git clone --recursive` builds the game the same as today.
2. **Launcher skeleton.** Add Walnut through CMake (not premake) and port it to Linux
   (GLFW/Vulkan, fonts, paths). Add an empty app window in the game's look, the core
   library, tests and the formatting/lint configs.
3. **Setup wizard.** ISO/folder picker, XISO extraction, XBE verification, tool detection,
   one-click install (winget / VS bootstrapper on Windows, the distro package manager via
   pkexec on Linux, with the commands shown if that's unavailable), then build with
   progress, log and resume.
4. **Play and Settings.** Port every settings page and the save/profile editor from the
   C# launcher, plus the language (en, pt-BR, es, fr, de, it) and diagnostics.
5. **Switch over.** Delete the C# launcher, its build script and its tests. Update the
   README for players (three steps) and devs (one section). Update release packaging and
   CHANGELOG.

## Things to know

- Players need a Vulkan-capable GPU driver, which any GPU from the last ~10 years has.
  Developers building the launcher need the Vulkan SDK; the launcher's CMake can fetch the
  headers and loader so the full SDK is optional.
- The VS Build Tools install is about 3–5 GB and needs administrator rights once. That can't
  be avoided while the game has to be compiled on the player's PC.
- Phases 1 and 2 are independent and could run in parallel sessions. Phases 3–5 build on 2.
