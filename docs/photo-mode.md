# Photo mode

**Pause > PHOTO MODE** freezes the mission and gives you a free camera with a small panel in the game's menu style.
Photos are saved as PNG files at the internal resolution, without the HUD, the menus or (by default) the
first-person weapon.

## Using it

| | Keyboard and mouse | Controller |
|---|---|---|
| Move | W A S D | Left stick |
| Look | Mouse | Right stick |
| Down / up | Q / E | Left / right trigger |
| Faster / slower | Hold Shift / Ctrl | – |
| Zoom (field of view) | Wheel | White (wider) / Black (narrower) |
| Roll | Z / C | Panel row |
| Take a photo | F12 | Y |
| Hide or show the panel | H | X |
| Panel | Arrows, Enter, Escape | D-pad, A, B |
| Leave photo mode | Escape / EXIT PHOTO MODE | B or Start |

The panel also sets the **field of view**, **roll** (five-degree steps), whether the **weapon** is shown,
the **camera speed** and the **camera range**: 6, 12, 20 or 30 metres around where photo mode started (and half as
high), so the camera cannot leave the built level. The panel fades while the camera moves and comes back when it
stops; any panel input brings back a panel you put away. **RESET CAMERA** returns to the gameplay view.

Photos go to `Pictures\BLACK` as `BLACK_<date>_<time>.png` (`RECOMP_PHOTO_DIR` moves them). The panel shows each
photo's file name, or that it could not be saved. Photo mode refuses to open while a scripted camera runs.
The game's motion blur and its reload and aim blur are off while photo mode is on, so moving the camera never
smears a photo.

## How it works

| What | Where |
|---|---|
| Camera, controls, limits, photo names | `pc_photo.c` (xboxrecomp `src/video`) |
| The panel: `photomode = on` pages, `photo = TEXT \| fov/roll/weapon/speed/range` rows, `photo_shot`, `photo_reset`, `photo_hide` | `pc_menu.c`, `[page PhotoMode]` in `src/black_native_menu.inc` |
| Input ahead of the menus: filter slot 3, mouse motion to filters, pointer capture outside gameplay | `pc_input.c` |
| PNG capture of the next finished frame (WIC) | `kgpu_request_capture` in `kelvin_gpu_post.inc` |
| The game: camera replacement after `sub_000D2F40`, field of view through the view window, weapon hiding, blur off, when photo mode may open | `BLACK_PHOTO_MODE` and the hooks around it in `src/recomp_manual.c` |

The free camera starts from the gameplay camera's matrix. World Y is up, and the game's handedness is read from that
matrix. While the PhotoMode page is shown, `pc_photo_camera` replaces both RwCamera frame matrices after the game
produces its world camera every update. The game rebuilds them from the camera entity each update, so leaving
photo mode needs no restoring. The weapon is hidden by scaling the first-person draws (found by
`BLACK_VIEWMODEL_OWNER`) to nothing. The [photo tests](../tests/photo/README.md) cover the camera, limits, controls,
photos and the panel without game files.

## Feasibility findings (Phase 0)

| Question | Result |
|---|---|
| Does the paused game keep rendering the 3D scene? | Yes. The world camera producer `sub_000D2F40` runs about 60 times a second while paused, and every frame is redrawn. |
| Where can the camera be replaced? | Right after `sub_000D2F40`. It copies the camera entity's matrix into the RwCamera frame (`[camera+0x58]+4`: modelling matrix at `+0x10`, LTM at `+0x50`; rows right/up/at/pos of 16 bytes) and then sets the view window. Rewriting both matrices afterwards moves the rendered view, and the frustum used for culling follows it. |
| Is geometry culled by the player's position? | No. Views 8, 20 and 40 units ahead, 10 behind and 25 above all render the loaded level, including enemies frozen in place. |
| What limits are needed? | Distance and height. Past the built level the camera shows black voids and the outsides of walls. |
| HUD and menus in captures? | Neither. The game hides its HUD while paused, and the native pages are drawn by the presenter, outside the captured frame. |
| Cutscenes and scripted cameras? | The camera entity's FOV-enable byte `+0x16A4` is clear in scripted camera mode (`BLACK_PC_FOV`); photo mode refuses to open then. |

## Later phases

- **Lens and look:** depth of field with manual focus and aperture, exposure, contrast, saturation, temperature,
  vignette, film grain and colour fringing in `kgpu_hlsl_post` (after depth of field, before the HUD), and filters.
- **Framing:** letterbox ratios, borders, a BLACK logo stamp, and a rule-of-thirds grid that is never captured.
- **Capture quality:** 2x and 4x photos, rendering one still frame at a higher internal resolution.
- **Polish:** collision with walls, smoothed camera motion, saved presets.
