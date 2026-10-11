# Native model lookup observations

`RECOMP_BLACK_WEAPON_TRACE=1` also enables at most 128 `[WEAPON_MODEL]` records.
Stage 1 observes the original AI gun-model resolver's result. Stage 2 follows
the native write to model slot zero, before later count/render flag updates.
Records include the original and requested compact-name tokens, returned asset,
validated slot pointer, slot-zero/current entries, selected index and flags.
The observer never writes guest memory or replaces a native decision.

Configure/build this directory with CMake and run CTest. The actual authored
helper is extracted into the build tree and receives synthetic memory only.
The test passes 321 state/memory checks enabled and 321 disabled, verifies the
128-record cap, all host rounding modes and MXCSR status bits, invalid actor and
slot spans, invalid selected index, and integer-only name logging. Generator
tests independently enforce the lookup/write boundaries and idempotence.
