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
guard, verifies unchanged memory and floating-point state for 272 calls, rejects
out-of-range pointers/slot indices, treats NaN positions as raw bits, and checks
the 256-record cap and the disabled path. No game files or process are involved.

The idempotent `scripts.generate.weapon_trace_hooks` adds the same observation
sites to existing locally generated C when a diagnostic rebuild must preserve
the baseline. Such a chunk also needs the declaration
`extern void black_weapon_trace(uint32_t, uint32_t);`.
