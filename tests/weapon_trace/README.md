# Native drop diagnostics

`RECOMP_BLACK_WEAPON_TRACE=1` records at most 256 native drop boundary events.
Stage 1 is actor cleanup (which can schedule a pending death), 2 is the actor's drop policy check, 3 is a
weapon spawn attempt, and 4 is a constructed and spatially registered weapon.
Stage 1 can follow an already completed drop pass; other native paths also set
the pending-death flag. Actor records include the guarded slot pointer, all
three model entries, current slot/count, render flag, asset and raw position.
An empty current model slot is a native no-drop gate. Offscreen/scripted deaths
must be distinguished from a visible enemy killed by the player before calling
that empty slot a bug.
Policy/disabled flags and allocator decisions remain native. This diagnostic is
not a gameplay repair or proof that a weapon was visible or collected.

With MSVC, configure `cmake -S tests/weapon_trace -B build-weapon-trace`, build
Release, and run `ctest --test-dir build-weapon-trace -C Release --output-on-failure`.
Alternatively run `python tests/weapon_trace/run.py --output <private-build-directory>`
with MinGW GCC available. The fixture extracts the current authored helper and RAM
guard, verifies unchanged memory and floating-point state, rejects
out-of-range pointers/slot indices, treats NaN positions as raw bits, and checks
the 256-record cap and the disabled path. No game files or process are involved.

The idempotent `scripts.generate.weapon_trace_hooks` adds the same observation
sites to existing locally generated C when a diagnostic rebuild must preserve
the baseline. Such a chunk also needs the declaration
`extern void black_weapon_trace(uint32_t, uint32_t);`.

The same switch also enables `[WEAPON_ATTACHMENT]` observations at player
submission (stage 1), NPC submission (stage 2) and the separate NPC suppressor
draw (stage 3). These report the native weapon's `+108` suppressed flag and the
correct model source for each path; they do not set the flag or replace the
model. A 128-entry actor/stage cache emits only changed snapshots, capped at 128
records. The separate NPC `BG1_SPR` model never gains player FOV compensation.

`RECOMP_BLACK_VIEWMODEL_TRACE=1` also emits `[MUZZLE_GATE]` when native flash
state changes, capped at 64 records. Native drawing requires frame 0–2,
texture 0 present, and that frame's nonzero size and opacity. The selected
texture variant is reported separately. This identifies native rejection
before the exact-player projection upload without relaxing its ownership.
Hundreds of repeated idle snapshots cannot exhaust either trace budget.

The synthetic fixture covers nested pointer bounds, invalid frame/variant
bytes, distinct player/NPC model layouts, raw signaling-NaN size bits, idle
deduplication, bounded output, disabled diagnostics, whole-RAM preservation,
and all host rounding modes and sticky flags. Generated chunks also require
the declarations `black_weapon_attachment_trace(uint32_t, uint32_t)` and
`black_viewmodel_muzzle_gate(uint32_t)`; the generator supplies them.

For an isolated offline scope lab only, `RECOMP_BLACK_TEST_WEAPON=svd` selects
the existing `BG1_SNR` token at player loadout initialization in Naszran Town
or Vratska Dockyard. Native model loading, weapon construction, animation and
inventory resolution then run normally. It requires the current player's
initialization record, one matching token in the loaded gun table, valid
descriptor/clip/ammo fields, and ordinary mission loadout mode. Missing,
different-case or unknown settings leave all guest bytes unchanged. This
control supplies test equipment; it does not verify natural weapon pickup.

The fixture covers absent/unknown settings, non-player record rejection,
wrong mission/mode, invalid inventory/table pointers, invalid count/index
domains, duplicate/missing tokens and invalid descriptor fields. Accepted
selection changes exactly the eight primary-token bytes and preserves every
other RAM byte plus all host rounding modes and sticky flags. The guarded
generator hook runs before the normal secondary-token/inventory initialization.

`RECOMP_BLACK_TEST_UNIT=2` selects the second native streaming unit in an
isolated Naszran Town or Vratska Dockyard lab (stage 1). Unit identifiers are
literal `StUnit02.bin` numbers. The loaded mission header
bounds the selection; unknown/malformed settings and invalid pointers do nothing.
The native streaming and player initialization paths then use their stock
checkpoint data. The test checks the sole four-byte unit-index write and exact
RAM/FP preservation. This control supplies a test starting point; it does not
represent completed gameplay or save progression.
