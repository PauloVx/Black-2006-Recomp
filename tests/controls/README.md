# Controls regression

These Windows tests compile the patched production menu and input sources
(`pc_menu.c`, `pc_input.c`, `pc_controls.c`) with the built-in native menu
definition extracted from `src/black_native_menu.inc`. They need no generated
guest code, display, controller or saved game.

```powershell
cmake -S tests/controls -B build-controls -A x64
cmake --build build-controls --config Release
ctest --test-dir build-controls -C Release --output-on-failure
```

The default runtime path is `third_party/xboxrecomp`. Override it with
`-DXBOXRECOMP_DIR=<absolute pinned, patched checkout>` for another checkout.

The test covers:

- keyboard rebinding: primary and secondary columns, a key leaving the action
  that had it, clearing, reserved Escape, and the bracketed prompt text
  following the new primary key;
- controller rebinding against four synthetic presets (no retail control table
  is copied): swapping with the listed action that had a button, START and
  movement refused, restoring the layout's button, the layout wrapping and
  replacing per-action changes, and the live map written on each change;
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
