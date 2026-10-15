# Photo mode regression

These Windows tests compile the patched production photo, menu and input sources (`pc_photo.c`, `pc_menu.c`,
`pc_input.c`) with the built-in native menu definition extracted from `src/black_native_menu.inc`. They need no
generated guest code, display, controller or saved game. The camera is a synthetic matrix, and the game's capture
is a stub that records the photo's path.

```powershell
cmake -S tests/photo -B build-photo -A x64
cmake --build build-photo --config Release
ctest --test-dir build-photo -C Release --output-on-failure
```

The default runtime path is `third_party/xboxrecomp`. Override it with
`-DXBOXRECOMP_DIR=<absolute pinned, patched checkout>` for another checkout.

The test covers:

- the camera: with no input it reproduces the game's matrix in either handedness; W moves along the view,
  mouse right turns right, E rises, and the range and height limits hold at the fastest speed;
- zoom from the wheel and its bounds, roll steps, the field-of-view value, and the weapon row;
- photos: a `BLACK_*.png` path in `RECOMP_PHOTO_DIR`, the shutter, and the saved / failed messages;
- leaving: the game's camera and field of view are untouched again;
- the panel: PHOTO MODE refused while the game says no (a scripted camera), opened from the pause menu, the
  panel's rows, a field-of-view change, a hidden panel drawing nothing and coming back on input, and Back
  returning to the pause menu.

With `BLACK_GAME_DIR` set to your extracted game files, the panel is also rendered with the game's own fonts to
`build-photo\photo-*.bmp`.
