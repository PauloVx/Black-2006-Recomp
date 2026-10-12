# Black 2006 Recomp

An experimental native Windows recompilation of the original Xbox release of
**BLACK (2006)**, built with XboxRecomp. It includes a Direct3D 11 renderer,
keyboard and mouse controls, XInput controller support, video settings, a PC
launcher, and native audio output. Movies are enabled by default.

Startup logos skip on one press of Escape, Enter, Space, left click, or controller
A / B / Start. During a cutscene, the first press shows **Press again to skip**
in the bottom-right corner. Release and press again within three seconds to skip;
holding a button cannot confirm. The hint also works with native menus disabled
and stays at the window corner on ultrawide displays. Looping menu backgrounds
keep playing normally. See the [movie skip tests](tests/movie_skip/README.md).

Ambient occlusion adds contact shading from scene depth with **SSAO Classic,
HBAO, HBAO+, and GTAO**, each with Low, Medium, High, and Ultra quality. Choose
the method and quality on the launcher's **Graphics** tab or the in-game
**Video Settings > Graphics** page. AO defaults to **Off**, with **High** quality
selected for when it is enabled. The controls and help text are in English.
These are local implementations of the techniques; HBAO+ does not use the
vendor SDK. The [AO regression tests](tests/ambient_occlusion/README.md) exercise
the actual shaders, settings, launcher controls, and native camera hook without
game files.

AO reconstructs positions at the sampled depth texel and retries when an early
draw has no usable depth buffer. Autofocus selects a representative surface in
the central aperture, reducing focus jumps from thin foreground objects.
After a gap without usable scene depth, autofocus reacquires the current view
instead of blending from an old foreground focus distance.
Wider gameplay FOV preserves the native framing of the player's weapon, arms,
and suppressor using their render-queue ownership. World and companion weapons
retain their own projection; see the [viewmodel tests](tests/viewmodel/README.md).
Ultrawide sniper scopes extend the original outer black masks to the screen
edges while preserving the aperture and reticle. The [scope mask tests](tests/scope_mask/README.md)
cover native resource ownership, aspect ratios and temporary draw-state restoration.

Enemy weapon-model lookup now preserves carry in the original 64-bit name
calculation. Corrupted model names previously left enemies without a weapon
model and prevented their native death-drop code from creating pickups. The
[weapon model regressions](tests/weapon_model_math/README.md) cover the arithmetic
and save-hash compatibility; native drop rules remain in control.

Launch through the first mission has been tested, including the opening movies,
mission briefing, movement, shooting, and reloading with audio. This is still a
work in progress: selected cemetery and docks scenes have also been checked,
but complete later-mission playthroughs remain unverified. Crashes and rendering issues
remain possible, and Xbox GP/EP audio effects are not fully emulated.

Gameplay voices and effects now honor their source sample rate at the 48 kHz
audio output. This corrects 32 kHz speech playing about 50% too fast and high.
The [APU regression test](tests/apu_pitch/README.md) checks pitch, duration,
stream completion, and playback lifecycle without game files or an audio device.

The fallback mixer preserves dry left/right and 3D speaker assignments without
playing raw effect sends a second time. This avoids reversed duplicate channels
and the clipping they caused. Native DSP output still takes precedence; see the
[spatial audio regressions](tests/apu_spatial/README.md). To export speech or
music from your own Xbox SSH/BKS banks, use the [audio extraction guide](docs/audio-extraction.md).

Audio pacing retains fractional frame time and recovers ordinary scheduling
delays. The Windows output queues two packets before starting playback to provide
scheduling headroom, and restores that buffering after an underrun. The [timing](tests/apu_timing/README.md) and
[output queue](tests/apu_output/README.md) regressions cover these paths without
an audio device. A reported critical device error recreates the output on its
submission thread and primes fresh samples; failed retries are limited to once
per second. Set `RECOMP_APU_TRACE_OUTPUT=1` for five-second source, clipping,
queue, playback and recovery counters when investigating an audio problem.

This repository contains host source and build tooling. **You must supply your
own original Xbox game files.** Game assets, the XBE, generated game-function
bodies, recordings, and prebuilt executables are excluded.

## Build

Requirements:

- Windows x64, an AVX2-capable CPU, and a Direct3D 11 graphics device.
- Visual Studio 2022 with **Desktop development with C++** and a Windows SDK.
- Git, CMake 3.20 or newer, and Python 3.10 or newer (`py -3`).
- Rustup with the `x86_64-pc-windows-msvc` toolchain (the pinned DSP build uses Rust 1.98.1).
- The supported retail original Xbox release of BLACK, extracted into `game/`.

The supported `game/default.xbe` has SHA-256:

```text
DF2739C372D254A90AEECCF5971D12097F22D89FBB90DB021FDA96DD4DEB68F6
```

Keep the extracted disc layout intact, including `GlobData.bin`, `levels/`,
`sound/`, and `videos/`. Other releases and the PlayStation 2 version are not
supported by this build.

From PowerShell in this repository:

```powershell
.\scripts\setup.ps1
.\'BLACK PC Launcher.exe'
```

Setup fetches pinned XboxRecomp and DSP56300 revisions, applies the included
runtime patch, creates a local Python environment, generates code from your XBE,
and builds the game and launcher. These dependencies and outputs stay outside Git.
Building the generated C code can take several minutes.

Use the launcher to configure controls and video settings, then choose **Play**.
Leave **Skip movies** and **Advance startup menus automatically** unchecked to
watch the full opening and navigate manually. Local settings live in `local/`
and diagnostic logs in `reports/`. Save-related data uses `save/` as well as
`game/UDATA/` and `game/TDATA/` through the native partition mapping. Close the
game and back up all three data directories before moving or removing the
project folder if you want to keep your progress.

AO requires the GPU renderer. Every quality uses a full-resolution R8 mask and
depth-aware blur; higher quality increases sampling density and radius. AO
multiplies scene color before depth of field, FXAA, sharpening, and the HUD.
It uses the world camera's current half-frustum, captured with each queued draw
so asynchronous rendering keeps the correct camera. Missing camera data or an
incompatible depth surface bypasses the effect. Depth reconstruction uses a
calibrated approximation, and HUD separation follows the existing depth-state
heuristic. First-mission HBAO+ High and AO Off were checked in gameplay; later
missions remain unverified.

Saved video settings use `ao_method=off|ssao|hbao|hbao_plus|gtao` and
`ao_quality=low|medium|high|ultra`. `RECOMP_AO_METHOD` and `RECOMP_AO_QUALITY`
override and lock their respective controls for that run. Legacy `ssao` and
`RECOMP_SSAO` settings remain supported. Untouched older default menu layouts
upgrade to include both AO controls; customized layouts are preserved.

## Credits and licensing

Built on [XboxRecomp](https://github.com/sp00nznet/xboxrecomp) by sp00nz, with
xemu-derived hardware components and nv2a_vsh_cpu. The original Xbox behavior
was investigated using community hardware documentation, including
[XboxDevWiki](https://xboxdevwiki.net/) and Cxbx-Reloaded references.
Development included assistance from Claude Code and OpenAI Codex.

Original project code is MIT licensed. Third-party components and their
modifications retain their respective licenses; see [LICENSE](LICENSE),
[NOTICE](NOTICE), and [LICENSES](LICENSES). BLACK belongs to its respective
owners; this project is an unofficial community effort.
