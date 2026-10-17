# Checkpoint persistence

These tests execute the production `src/black_checkpoint.inc` against synthetic
guest RAM and real temporary files. No game files or saved progress are needed.

Configure with `cmake -S tests/checkpoints -B build-checkpoints`, build Release
or Debug, then run `ctest --test-dir build-checkpoints -C Release --output-on-failure`.

The harness launches separate writer and reader processes with different guest
allocation addresses. It checks inventory, ammunition, health, objective and
statistic snapshots, native streaming selection, and the Continue Mission gate.
It preserves the native allocation pointers and floating-point environment.
Truncated, extended, damaged, incompatible and incorrectly bound files leave
all guest RAM unchanged. Failed atomic replacement preserves the previous save.
Native profile autosave dates are rebound only at the successful completion
boundary; changed campaign progress retires the checkpoint. Saves before the
first capture and between captures keep the latest checkpoint bound to the
successfully written profile date. Both full mission restart callbacks discard
it. A live in-session checkpoint remains authoritative.

The generator regression pins both load and profile-save success boundaries and
rejects changed or duplicate labels. The tests mock generated guest bodies;
they do not establish natural checkpoint activation, death or visual respawn.

The format is versioned and tied to the supported retail XBE's native record
layout. It serializes 80 player bytes, bounded inventory ownership, objective
records, and native statistic scalars. It excludes allocation pointers. Objective
identifiers and weapon tokens must match the newly loaded mission before any
guest state changes. Temporary files are flushed before atomic replacement.
