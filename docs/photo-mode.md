# Photo mode: feasibility (Phase 0) and design

Phase 0 checked, in the first mission, whether a free camera can work while the game is paused. It can.

## Findings

| Question | Result |
|---|---|
| Does the paused game keep rendering the 3D scene? | Yes. The world camera producer `sub_000D2F40` runs about 60 times a second while paused, and every frame is redrawn. |
| Where can the camera be replaced? | Right after `sub_000D2F40` (already hooked for FOV, `BLACK_PC_FOV`). It copies the camera entity's matrix into the RwCamera frame (`[camera+0x58]+4`: modelling matrix at `+0x10`, LTM at `+0x50`; rows right/up/at/pos of 16 bytes) and then sets the view window. Rewriting both matrices afterwards moves the rendered view, and the frustum used for culling follows it. |
| Does the camera need restoring? | No. The producer rebuilds the matrices from the camera entity every frame, so stopping the override returns the gameplay view. |
| Is geometry culled by the player's position? | No. Views 8, 20 and 40 units ahead, 10 behind and 25 above all render the loaded level, including enemies frozen in place. |
| What limits are needed? | Distance and height. Past the built level the camera shows black voids and the outsides of walls, and it passes through walls. |
| HUD and menus in captures? | Neither. The game hides its HUD while paused, and the native pause page is drawn by the presenter, outside the captured frame. |
| First-person weapon? | It disappears once the camera moves, since it is drawn attached to the gameplay camera. |
| Cutscenes and scripted cameras? | Not tested. The camera entity's FOV-enable byte `+0x16A4` is cleared in scripted camera mode (see `BLACK_PC_FOV`); photo mode should refuse to open while it is clear. |

The spike adds two diagnostics:

- `RECOMP_PHOTO_PROBE=BACK,UP,YAW[;...]` (`BLACK_PHOTO_PROBE` in `src/recomp_manual.c`): while paused, steps the world camera through up to eight offsets for three seconds each and captures `photo-probe-N.bmp`.
- `kgpu_request_capture(path)` (`kelvin_gpu_post.inc`): writes the next finished frame, at internal resolution and before window scaling, as a BMP. Photo mode's capture builds on it.

## Phase 1 design

- **Entering:** a PHOTO MODE item on the native pause page, refused during scripted cameras. The game stays paused underneath; photo mode hides the pause page and takes all input through its own `pc_input` filter, ahead of the menus. Back/Escape returns to the pause page.
- **Camera:** a host-side free camera starting from the gameplay camera's matrix: position, yaw, pitch, roll and FOV. It is written into both RwCamera matrices after `sub_000D2F40` while photo mode is active. Movement is clamped to a sphere around the player (and a height band) so it cannot leave the built level.
- **Controls:**
  - Keyboard and mouse: WASD, mouse look, Q/E down/up, Shift/Ctrl faster/slower, wheel for FOV, Z/C for roll.
  - Controller: both sticks, triggers for down/up, White/Black for FOV, d-pad for roll.
- **Panel:** native menu rows in the game's style, hidden while the camera is moving.
- **Capture:** `kgpu_request_capture` extended to write PNG through WIC to `Pictures\BLACK`, with a shutter sound and an on-screen note.

Later phases add lens and colour grading in `kgpu_hlsl_post` (after depth of field, before the HUD), framing overlays, high-resolution capture (render one still frame at a higher internal resolution), presets and localization.
