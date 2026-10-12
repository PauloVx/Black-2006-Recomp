# Audio timing regression

This suite compiles the patched runtime's `apu_timing.h`. It checks QPC scaling
after long Windows uptime, exact accumulated time for 90 minutes of 256-sample
48 kHz blocks, retention of ordinary scheduler debt, and bounded recovery after
a longer interruption. It does not claim a 90-minute audible gameplay test.

```powershell
cmake -S tests/apu_timing -B build-apu-timing -A x64
cmake --build build-apu-timing --config Release
ctest --test-dir build-apu-timing -C Release --output-on-failure
```

Use `-DXBOXRECOMP_DIR=<patched-runtime>` for an alternate runtime checkout.
The audio CI workflow runs Release and Debug configurations.
