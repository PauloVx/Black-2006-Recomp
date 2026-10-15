# Changelog

## Unreleased

**A new launcher in C++ (preview).** `launcher/` is a Walnut (Vulkan + Dear ImGui) application
drawn like the game's own menu pages: BLACK's logo, colours, rails and selection, and, once your
disc files are in place, the game's own menu fonts read from them. It shows whether the disc and build
are in place and offers Play, Settings, Open game folder and Quit; Play and Settings still hand over to
the current launcher. Build it with `cmake --preset windows-release` in `launcher/` (see
`launcher/README.md`); unit tests and a Windows/Linux CI workflow come with it.

**XboxRecomp is now a submodule.** `third_party/xboxrecomp` is PauloVx/xboxrecomp on its
`black` branch, where the runtime changes are ordinary commits; `patches/xboxrecomp.patch` and
`patches/xboxrecomp-clang.patch` are gone. The DSP56300 core moved from `.work/err009-dsp56300` to
the `third_party/dsp56300` submodule. Clone with `--recursive`; setup fetches missing submodules,
and release archives include both sources.

Vertex shader compiles no longer cause hitches when a level first appears. The
shader cache is keyed by the generated HLSL alone, so one program is compiled once
instead of again for each copy left in program memory (113 cached files were 18
programs). Each program's microcode is recorded in `vs_manifest.bin` in the cache,
and launch compiles any recorded program the cache lacks on all cores before the
game starts (`RECOMP_KGPU_PRECOMPILE=0` skips it). A program first met in play runs
a quick unoptimised build, about twice as fast, until its optimised build is cached
(`RECOMP_KGPU_VS_QUICK=0` disables it). Existing caches carry over.

**Options > CONTROLS** rebinds the keyboard, mouse and controller in game,
replacing the console's Controller Setup page. Keyboard and mouse actions take
a primary and a secondary key; the controller picks one of the four original
button layouts and moves single button actions, swapping with an action that
already had the button. Changes write the game's own action map, so prompts
show the new keys and button art at once. Bindings live in
`local/controls.ini`, shared with the launcher. Unedited `menus.ini` files
upgrade to the new pages automatically.

**Photo mode** (Pause > PHOTO MODE): a free camera over the paused mission with field of view,
roll, speed and range, controlled by keyboard and mouse or controller from a panel in the game's
menu style. Photos are saved as PNG to `Pictures\BLACK` without the HUD, menus or weapon. Game blur
effects are off while it is open; scripted cameras cannot be entered. Unedited `menus.ini` files
gain the new pause item automatically.

**Reflections**: with the GPU renderer, screen-space reflections replace the
game's low-resolution 128 x 128 environment maps on the surfaces that use them,
mostly the first-person weapons and some windows. Rays that miss use a blurred
copy of the screen instead of the baked map. Night scenes are brightened to match
the original's overall level. A new Reflections row (Original, Low, Medium, High,
Ultra; default High) is on the launcher's Graphics tab and the in-game
Video Settings > Graphics page. `RECOMP_REFLECTIONS` pins it. See
[docs/reflections.md](docs/reflections.md).

## 0.1.0-alpha.2 — 2026-10-14

**Build BLACK.sh** now generates and compiles the game entirely on x86_64 Linux
without a Windows installation. The build uses native Linux Python, LLVM,
Rust and Mono, with a pinned Microsoft SDK/CRT downloaded by xwin. It outputs the
Windows x64 game executable for Wine/Proton and builds both host launchers.

Added the CMake cross toolchain, compiler compatibility fixes, local language-bank
generation, build provenance, and an asset-free Linux runtime/Rust ABI CI check.
Retail files and generated game code remain local. Windows building stays supported.

## 0.1.0-alpha.1 — 2026-10-14

First public starter release. Includes compiled one-click **BLACK PC Launcher.exe**
and separate **BLACK PC Settings.exe**, Windows build shortcut, Linux Wine/Proton
entry points, host source, licenses, and archive/file SHA-256 checksums.

Portable folders may keep the locally generated game executable in
`bin/black_xbox_recomp.exe`; existing `build-vs2022/Release/` projects also work.
Play uses saved preferences and settings opens separately. Both share saves,
language selection and diagnostics. The latest audio completion freeze,
weapon pickups, save/profile, ambient occlusion and localized bank fixes are included.

The public starter requires your own supported Xbox files and a one-time Windows
build. Game executables, retail data, generated game code, language banks and saves
are excluded. Linux runs the resulting Windows build through Wine/Proton.
This remains experimental; full later-mission playthroughs are unverified.
