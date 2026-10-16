# Split-screen co-op (and later PvP)

Two players on one PC, each with their own view, playing BLACK's missions together. Online PvP comes later and
reuses the same "player" layer, with a network transport underneath. This page records what the engine looks like
from the recompilation, the approach, and the milestones in order.

## What the engine assumes

BLACK is a single-player game, and the code shows it:

| What | Where | Notes |
| --- | --- | --- |
| The player | `[0x2D45D4]`; its actor at `+0x30` (position `+0xA0..A8`) | Used in **908** places in the generated code. Set by `sub_0020BC40`, `sub_0020BD72` and `sub_0020BDF7` (spawn and restore paths). |
| The camera owner | `[0x2D12B8]`; camera entity `+0x740`, field of view `+0x750`, FOV-enable byte `+0x16A4` | Set by the same three functions. |
| The renderer | `[0x2D67F4]`; world camera `+0xD450`, raster `+0xD408` | One world camera. |
| Mission stats | `[[0x2D12CC]]` | One set of counters (bullets fired `+0x48`). |
| Characters | vtable `0x0028C1E8`, health float `+0x2F0`, death flag `+0x3AD` | Enemies and the player's own class. |
| Input | one emulated USB pad (`usb/usb_gamepad.c`) | The game polls the controller XAPI through emulated OHCI. |

### One frame

`sub_000E1A00` (the main loop) calls `sub_0020F6A0` once per frame:

1. **Update**: `sub_001F1080` and below. The gameplay look integrator `sub_001BD3F0` runs here
   (`sub_0020F6A0` → `001F1080` → … → `001EC450` → `001BD3F0`).
2. **Camera**: `[[0x2D12B8]]->vtbl[2]`. This reaches `sub_002033A0` (the camera owner's view: 14 camera modifiers,
   then the lens), which calls `sub_000D2F40` to copy the camera entity into the RwCamera frame.
3. **Draw**: `sub_000DAC10`, `[esi+0x21070]->vtbl[2]`, `[[0x2D4628]]->vtbl[1]`, `sub_001F9130`, then
   `sub_001762B0(renderer)` and `sub_001732A0(renderer)`.
4. **Show**: `sub_0002DD10(renderer+0xD408, 0, 1)` (one present per frame), then `sub_0020D400`.

Everything is drawn before a single present, so drawing the world twice in one frame is possible in principle.
Which of the step 3 calls draws the world (rather than the debug lines and the HUD) still has to be measured. Draw
counts between hooks in the GPU renderer will tell.

## Approach: player contexts

The game will not learn that there are two players. Each player gets a **context**: the values of the globals that
mean "the player" (`[2D45D4]`, `[2D12B8]`, the pad report, the HUD's state and anything else found on the way). The
frame tick then runs the player-dependent phases once per context, switching the globals around each phase:

- **Update**: P1's context runs the frame's update as now. P2's context runs again only the player's own update
  (input, movement, weapon and camera), not the world's. Splitting "the player's update" from "the world's update"
  inside `sub_001F1080` is the main research item.
- **Camera and draw**: for each context, run step 2 into that context's camera, then the world draw into that
  player's part of the screen.
- **Show**: once.

Hooks on guest functions are manual overrides in `src/recomp_manual.c` (`void sub_XXXXXXXX(void)` calling
`sub_XXXXXXXX_gen()`). A new override needs the game code generated again, because generation excludes the
manual functions.

### Views

The GPU renderer (`kelvin`) takes a **view region** for the draws that follow: a viewport and a scissor rectangle
in the full raster. Clears are scissored to the region. The game's own full-screen effects (motion blur, depth of
field) and the PC's passes (AO, SSR, ray tracing, DLSS) assume one view. They stay off in split-screen at first, then
run per region. Layouts: top and bottom (default) or side by side. Each view keeps the window's aspect ratio by
changing its field of view, as the aspect-ratio work already does.

### Input

Player 2 reads the second XInput controller directly on the host. Adding it as a second USB pad would let a
single-player game read it as player 1 too. During P2's context the emulated pad report becomes P2's, so the game's
own input code moves P2. Player 1 keeps the keyboard and mouse, or the first controller.

### Later: PvP

The same contexts carry remote players. A network layer (host-authoritative: clients send input, the host sends
state snapshots) fills a context from the wire instead of a local pad. The debug overlay already has a PING row,
waiting for `pc_debug_note_ping`.

## Milestones

| # | Milestone | Done when |
| --- | --- | --- |
| 0 | Debug overlay (Insert) | Done: frame times, latency and render numbers to measure the cost of a second view. |
| 1 | **Second view** | The world is drawn twice per frame, the second time from a free camera on the second controller (photo-mode style), into the bottom half. P1 plays normally in the top half. HUD in P1's half only. Frame rate stays at least 60 at 1440p on an RTX 3060 Ti. |
| 2 | **Player 2 joins** | START on the second controller spawns a second player character near P1, through the game's own spawn path (`sub_0020BC40` and the two others). P2 moves, aims and shoots with its own camera and view. |
| 3 | **Playing together** | Enemies target the nearer player. Damage, death (a downed P2 respawns at P1 after a few seconds), pickups, objectives and mission end work for both. Checkpoints and saves stay P1's. |
| 4 | **Polish** | A HUD, crosshair and hit markers per view. Audio from P1's camera, or the nearer one. Split-screen settings in game and in the launcher (layout, P2's controls). Leaving with START > Quit. |
| 5 | PvP groundwork | Contexts filled from the network, a lobby, snapshots and prediction, and the PING row. |

## Risks

- **Singleton state beyond the two pointers.** HUD widgets, weapon selection, objectives, the "player is in a
  scripted sequence" state and audio listeners may each have their own global. Each one found joins the context.
- **Update side effects.** Running parts of the update twice can double timers or AI ticks. The P2 pass must run only
  the player's own objects.
- **Cutscenes and scripted cameras.** These show full screen for both players. P2 is hidden or teleported to P1
  afterwards.
- **Performance.** Two world draws roughly double the GPU's work. The game itself is light (about 7 ms of GPU time at
  1440p today). The PC's extra passes are not, so they start off in split-screen.
