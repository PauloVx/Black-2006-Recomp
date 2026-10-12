# Weapon model name arithmetic regression

The native AI constructor appends `_LOD` to a compact weapon name before finding
the world-model asset. Two unsigned 64-bit CRT helpers contained four omitted
rotate-through-carry instructions. With the high half of the divisor nonzero,
the omitted operations corrupted quotient/remainder results. Real names such
as `BG1_AK1` and `BG1_SNR` consequently resolved to different model tokens.

The title generator repairs precisely those four missing rotations. It retains
the surrounding native normalization, correction branches, scratch registers,
callee-saved registers, stack cleanup and divide-by-zero behavior. Other
translated functions are untouched. Generator tests reject changed or ambiguous
insertion sites and verify repeated processing is a no-op.

Source-only CI uses an authored pair-of-registers normalization fixture generated
through the actual repair. A whole `uint64_t` oracle checks 300,804 results,
including every one-bit position, zero low halves and powers of 40:

```
cmake -S tests/weapon_model_math -B <private-build>
cmake --build <private-build> --config Release
ctest --test-dir <private-build> -C Release --output-on-failure
```

For a local game-owning build, `run.py --generated src/recomp/gen --output
.work/<private-build>` extracts the exact two generated helpers and their actual
name-concatenation caller. It compares 100,000 deterministic inputs plus boundary
and powers-of-40 cases to unsigned host arithmetic, checks stack arguments and
callee-saved registers, and checks the two real name tokens. This passed
1,302,092 checks after the repair. Before the repair, `--expect-broken` reproduced
six concrete arithmetic/name failures. Extracted functions remain private build
outputs; they must never be committed or distributed.

`--save-compat` tests the separate save-name hash divisor `2^48-59` against
1,310,720 boundary inputs. The old and corrected helpers both return the same
correct arithmetic there: the old normalization can overestimate its quotient
by at most one and cannot underestimate it, so the retained native correction
recovers the remainder. This is distinct from the powers-of-40 asset-name defect.

This arithmetic evidence establishes the asset-name defect. The opt-in native
constructor/death/drop trace provides separate in-game acceptance evidence.
