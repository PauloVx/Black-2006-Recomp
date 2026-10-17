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
The world is `sub_001EE820(player)` (stdcall, one argument), reached through `[esi+0x21070]->vtbl[2]` →
`sub_0020BAC0` (or `20BB40` / `20BBC0`). That caller clears (`sub_00268880(0, NULL, 0xF3, 0, 1.0f, 0)`), draws the world,
then draws the HUD through the 2D camera. `sub_001762B0` begins a camera by index in eax: 0 is renderer+0xD3B0 (2D),
1 is renderer+0xD450 (the world), 2 is renderer+0xD4F0. `sub_000D2F40` takes only edi (the camera). The D3D device is
`[0x27BFF8]`, and its first dword is the push pointer.

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

Each view is a **whole frame of its own** (`kelvin_mark_view`, a mark at the D3D push pointer so the GPU switches at
that command). The game draws it as a full 640x480 frame with the camera's aspect doubled: the horizontal field of
view stays and the vertical halves. The GPU draws it on main-size surfaces of that view's, at its share of the raster.
When the view ends, its ray tracing, AO, reflections and DLSS/DLAA run as at the end of a frame, with that view's own
history (one NGX feature per view, RT history copied in and out), and the view is copied into its half. The game's
own effects (glow, action blur) work per view as a result. A first attempt drew both views into one frame through a
cropping viewport. Every full-screen pass then saw both cameras at once, and the result was a ghost of the other
view. The game's camera-motion blur stays off in split-screen: it keeps one camera history, which belongs in the
player context. Layout: top and bottom (side by side later).

### Input

Player 2 is a second emulated USB pad, plugged in once the first is configured. Its report comes from the second
XInput controller, or from player 2's keys (arrows for the left stick, the numpad for the rest), and the game decodes
it into input component 1 of `[2D45E8]`. Player 2's controller reads that component. Player 1 keeps the keyboard and
mouse, or the first controller.

### Player 2

The mission core `[2D45D4]` (0x5CC0 bytes) holds exactly one player actor at core+0x30 (0x900 bytes). The engine has
a cut two-player mode (`sub_001F4540` sets two players, and the player loops step by 0x900), but the core has no room
for a second actor. So player 2 is swapped into core+0x30 whenever its code runs, and swapped out afterwards:

- **Out of the core**, player 2's actor lives in guest memory (the shadow). It owns player 2's physics proxy (DF400,
  one of the world's 20 character proxies), so the world's step and contacts reach player 2's own data.
- **Kept with the core**: the actor's world-grid node (+0x20..+0x57) and any field whose list neighbour points back
  at it. The draw does not use the node for the players: each view draws its own player after the grid walk.
- **Swapped with the actor**: the camera owner `[2D12B8]`; player 2's copy of the first-person weapon state (the
  viewmodel set `[2D12C8]`: its animation sets and two slots, each slot's locator records and animation controller,
  and the animation-pool blocks the controllers use; exchanged in place, since the loaded animations point back into
  it), with the animation library's working state (`0x2C3260..0x2CE710`); player 2's own first-person weapon files:
  a manager of its own for `[2D12D8]` (which reads a weapon's file into the next of two heaps, 13 and 14) reading into
  heaps 16 and 18 (unused by the game) on memory of its own, and its own copy of the first-person sound pool
  (`[[2D0D60]+0xD3C8]+0xC`) with its four weapon sounds and two banks on memory of its own; the first-person muzzle flash (`[2D1990]+0x4E1F0`, drawn at the in-view player's Flash_Loc) and shells
  (`+0x506D0`, thrown from Bullet_Loc); and, in the effects held by characters (`+0x65690`), the owner of the
  out player's (the shadow), so each view leaves out the other player's first-person effects.
- **Own copies**: the model the actor is drawn with (+0x320, whose first entry is the weapon in hand), and what it
  could use or pick up (`[2D0D64]+0x5848`).
- **Per frame**: player 2's controller, actor update, and the post-physics update (actor vtable +0x10, which sets
  the eye matrix the camera follows) run after player 1's, then its muzzle flash ages, its shells move, as player 1's
  do in the effects' update, and the use query (1EAF40) finds what it could use. The held effects update once per
  player, with that player in.
- **HUD**: the elements that show the player (the health and ammo panels, 290F9C, 290F64, 290EEC) keep player 2's
  own state (each element's whole state: the health panel's reaches 0xBC0 bytes), updated with player 2 in and the
  frame's time; what each player's update put in those elements' sprites (the health bar's among them, only the
  sprites both players' states hold in the same place) is its own,
  and the ammo counter's sprites are each player's. The pack count in the HUD movie ("HealthPacks", one movie for
  both halves) is set to each half's player's count (actor +0x8E1) for its half. The messages element (the use
  prompt and what was picked up, all posted by the player in the core) is player 2's own, exchanged for its update
  and its HUD, and its prompts show player 2's pad buttons. The mission's texts (objectives, title cards) are shared:
  both halves show them.
- **Scripted scenes**: when the mission drives player 1 with the scripted controller (+0x324 = +0x800) and stops the
  first-person idle (slot +0x234), player 2 follows; joining during a suppressor fitting, its weapon starts in the
  same state; joining after the opening scene asked for the clip that readies player 1's weapon (kind 6), player 2
  asks for its own.
- **Crosshair and hit markers**: each player has its own spread and markers, and each turns into the game's dot
  when that player zooms. A marker shows only when that player's
  bullets hurt a soldier (1E74B0's attacker), not when soldiers hurt each other.
- **Own weapons**: player 2 starts with an empty weapon array of its own, so its load takes its own weapons.
- **Seen by the other player**: each player has a puppet, a copy of a level soldier in a free character slot, kept
  at that player's position (its transform without the fourth words player 1's actor keeps there), posed each frame
  (standing or moving clip), and drawn only in the other player's view; a puppet the game takes for a character of
  its own is made again in another free slot.
- **Characters in both views**: the viewmodel set's animation pool holds every animated model's blocks (a model's
  is its +0xAC), the level's characters too; only the first-person slots' blocks and tops are exchanged with the
  actor, so the characters keep their poses while player 2 is in (its update and its view).
- **Enemies**: player 2's entry in the AI's targets ([2D1988]+0xFAC, its index at the actor's +0x378) names player
  2's own actor. Not done: enemies still do not engage player 2. Its actor has no node in the world's grid
  (core+0x4960 holds one player node, the core's), and the soldiers' combat falls back to the core's player.

### Later: PvP

The same contexts carry remote players. A network layer (host-authoritative: clients send input, the host sends
state snapshots) fills a context from the wire instead of a local pad. The debug overlay already has a PING row,
waiting for `pc_debug_note_ping`.

## Milestones

| # | Milestone | Done when |
| --- | --- | --- |
| 0 | Debug overlay (Insert) | Done: frame times, latency and render numbers to measure the cost of a second view. |
| 1 | **Second view** (done: 69–72 FPS at 1440p with DLAA, GTAO, SSR and RT on; one view 69–88) | The world is drawn twice per frame, the second time from a free camera on the second controller (photo-mode style), into the bottom half. P1 plays normally in the top half. HUD in P1's half only. Frame rate stays at least 60 at 1440p on an RTX 3060 Ti. |
| 2 | **Player 2 joins** (done: chosen before the difficulty, as "Two players (experimental)" while enemies do not engage P2; P2 joins beside P1 when the mission starts) | P2 moves, aims and shoots with its own camera, view, weapons and HUD, and each player sees the other as a soldier. |
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
