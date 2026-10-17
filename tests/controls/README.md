# Controls regression

These Windows tests compile the patched production menu and input sources
(`pc_menu.c`, `pc_input.c`, `pc_controls.c`) with the built-in native menu
definition extracted from `src/black_native_menu.inc`. They also extract and
compile the actual `BLACK_PC_CONTROLS` block from `src/recomp_manual.c`; a small
synthetic guest-memory fixture supplies four preset tables and a generated-map
stub so the real preset reader and live-map writer run without game assets.
They need no generated guest code, display, controller or saved game.

```powershell
cmake -S tests/controls -B build-controls -A x64
cmake --build build-controls --config Release
ctest --test-dir build-controls -C Release --output-on-failure
```

The default runtime path is `third_party/xboxrecomp`. Override it with
`-DXBOXRECOMP_DIR=<absolute pinned, patched checkout>` for another checkout.
For regression negative controls, `-DBLACK_MANUAL_SOURCE=<file>` selects the
manual source whose wrapper is extracted, and
`-DBLACK_PC_CONTROLS_SOURCE=<file>` selects the controller implementation.

The test covers:

- keyboard rebinding: primary and secondary columns, a key leaving the action
  that had it, clearing, reserved Escape, and the bracketed prompt text
  following the new primary key;
- controller rebinding against four synthetic presets (no retail control table
  is copied): swapping with the listed action that had a button, START and
  movement refused, restoring the layout's button, the layout wrapping and
  replacing per-action changes, and the live map written on each change;
- the extracted production wrapper preserving control 28 (`PCC_CONTROLS`, the
  retail "none" sentinel) in slot zero while writing all 37 slots, applying a
  Fire/Reload swap to guest memory, restoring Fire with its displaced Reload
  action, clearing persisted overrides, and cycling to another preset;
- a fresh-process regression that loads a synthetic `FIRE=RT` override equal
  to the current profile default, restores Fire, confirms the saved override
  clears, and then verifies a later profile change updates Fire to its new
  default;
- `controls.ini`: the `[keyboard]` and `[controller]` sections as the game
  writes them;
- the native page flow: Options > CONTROLS > Keyboard > Movement, a key capture
  on the secondary column, Escape cancelling a capture and Delete clearing.

With `BLACK_GAME_DIR` set to your extracted game files, the pages are also
rendered with the game's own fonts to `build-controls\controls-*.bmp`:

```powershell
$env:BLACK_GAME_DIR = "$PWD\game"
.\build-controls\Release\controls-test.exe build-controls\menus.ini build-controls
```
