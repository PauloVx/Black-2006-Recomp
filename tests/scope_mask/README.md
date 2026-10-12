Runs the current authored scope-mask guard and wrapper on a synthetic relocated
APT resource graph, without a game launch, game code, or game assets.

Checks 21:9 and 32:9 outer-edge coverage while preserving the inner edges and all
Y fields; 4:3, Original and 16:9 pass through exactly. Wrong movie/leaf/geometry,
stencil passes, signalling NaNs, invalid pointers and aliasing fail closed.
The fixture drives the recovered cdecl shape slot (0x562BD8 -> 0x121B50), including
argument order and caller cleanup. The sibling glyph route is a negative control;
generation also verifies the native registration and shape consumer.
The wrapper calls the original once, restores all temporary RAM, preserves its
returned guest registers and MXCSR, and the helper preserves x87 environment.

```
cmake -S tests/scope_mask -B .work/scope-mask -A x64
cmake --build .work/scope-mask --config Release
ctest --test-dir .work/scope-mask -C Release --output-on-failure
```
