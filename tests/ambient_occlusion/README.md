# Ambient occlusion regressions

These tests use the authored production launcher and the pinned XboxRecomp source after
`patches/xboxrecomp.patch` has been applied. They require Windows, Visual Studio 2022's C/C++
tools, Python and the .NET Framework compiler. Retail BLACK files are not needed.

```powershell
cmake -S tests/ambient_occlusion -B build-ambient-occlusion -G "Visual Studio 17 2022" -A x64
cmake --build build-ambient-occlusion --config Release
ctest --test-dir build-ambient-occlusion -C Release --output-on-failure
cmake --build build-ambient-occlusion --config Debug
ctest --test-dir build-ambient-occlusion -C Debug --output-on-failure
```

The workflow first compiles the production renderer, video and input libraries, then runs:

- The actual video settings and menu row services: all AO methods and qualities, INI round trips,
  validation, legacy SSAO conversion, per-field environment locks, persistence, cycling and Reset.
- Native page parsing, input and state against a fake game host, including both English AO rows.
- The actual embedded pixel shaders on D3D11 WARP: all 16 enabled method/quality combinations,
  unoccluded flat/sky depth, an occluded corner, finite float masks, R8 masks, bilateral blur,
  alpha-preserving scene darkening, distinct kernels, quality differences and camera sensitivity.
  Existing DOF, FXAA, sharpening and SSAA shader entry points also compile.
- The actual launcher model and controls: all choices, file/JSON persistence, legacy and invalid
  inputs, culture independence, in-game editability, English labels and saved-choice reload.
  Language switches preserve edited numeric and text settings in both the UI and saved JSON.
  Synthetic language banks cover first-run English preservation and Portuguese/English switching.
- The authored camera hook and guard regressions in `camera/`.

WARP uses synthetic depth and scene textures; it does not execute the complete guest game or
prove HUD timing in gameplay. Camera/frame queue behavior and final scene appearance still need
real-game verification. Native font/image rendering and the optional historical default-menu
migration checks are skipped without their privately owned input files. No retail assets or
generated guest source are included in this test directory.
