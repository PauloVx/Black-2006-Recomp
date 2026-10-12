# First-person projection ownership checks

The authored title hook follows native first-person render submissions through
the material queue. Both the held weapon/arms and its suppressor carry the same
actor parameter. Native NPC/world submissions are excluded even when they use
the same vertex shader. Compensation changes only the temporary X/Y projection
constants being uploaded, preserving depth, W, animation and camera state.
The separate player muzzle flash receives the same correction at its immediate
matrix upload. That boundary requires the exact native flash call site, the
current player identity, and the primary camera; other immediate effects retain
their world projection.

`cmake -S tests/viewmodel -B <private-build>` then build and run CTest. The test
extracts the actual authored helper from `src/recomp_manual.c` into the build
directory. It uses synthetic memory; no retail assets or translated code.

Coverage: 9,912 checks each with compensation enabled/disabled, all rounding
modes and MXCSR sticky bits, shared shader ownership exclusion, nested draws,
queue reset, secondary cameras, Original-FOV reset, Z/W preservation, invalid
constants, bounds and overflow rejection. Muzzle tests additionally reject an
unrelated caller, a different player, invalid player memory and a secondary
camera while preserving nested draw state. `scripts/test_generate.py` checks the
insertion points fail closed and repeat without duplicate hooks.

Runtime acceptance remains separate: compare Original and high FOV at the same
pose with `RECOMP_BLACK_VIEWMODEL_TRACE=1`; `RECOMP_BLACK_VIEWMODEL=0` disables
the compensation for A/B capture. Check suppressed/unsuppressed weapons and
nearby companions across switching, reload, aim and the docks scene. Shader-hash
overrides (`RECOMP_BLACK_VM_VS`) are no longer used because they cannot establish
which actor owns a draw.

Local first-mission captures at 21:9 compared Original with FOV 80 at the same
room and opening poses. The pistol retained its screen framing while the world
view widened; the trace confirmed player-owned uploads with a 1.775346 ratio.
This does not cover the docks weapons, companion suppressors, or all firing
effects. Those scene-specific checks remain necessary.
