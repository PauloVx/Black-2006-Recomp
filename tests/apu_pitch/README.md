# APU pitch regression

The standalone voice processor had computed Xbox 4.12 pitch but discarded its
ratio and emitted one source frame for each 48 kHz output frame. A source at
24 kHz therefore played twice as fast and one octave high; BLACK's traced
`0xF6A4` pitch (about 32 kHz) played about 1.5 times as fast.

This isolated native test compiles the **production** `apu_vp.c`, including its
sample fetch, ADPCM decoder, resampler, PIO methods and VP frame mixer. It needs
neither game data nor an audio device. PCM output is compared with an independent
absolute source-position oracle. ADPCM fixtures use valid zero-nibble blocks
whose nonzero predictor remains constant, giving known decoded output without
reimplementing the decoder.

```powershell
cmake -S tests/apu_pitch -B build-apu-pitch -A x64
cmake --build build-apu-pitch --config Release
ctest --test-dir build-apu-pitch -C Release --output-on-failure
```

The default toolkit path is `third_party/xboxrecomp`. Override it with
`-DXBOXRECOMP_DIR=<absolute checkout path>` when testing another pinned, patched
checkout. The GitHub Actions workflow prepares this dependency and builds the
APU library and this test without retail game files or generated guest code.

Coverage includes bit-exact unity, mono/stereo 22.05/24/32/44.1/96 kHz pitch,
the real guest pitches `-2396` and `-501`, source consumption and duration,
fractional interpolation, arbitrary output chunk splits, inclusive EBO,
static tails, nonzero LBO loops, pause/resume, pitch changes, CBO seeks,
VOICE_ON restart, SSL joins, deferred DONE notifications, persistent starvation,
nonpersistent stream completion and final-frame mixing. Mono/stereo ADPCM tests
cross decoded block boundaries at native, half and fractional source rates.

The IDLE-trap regression keeps a continuing sound before and after two finished
voices, plus active sounds in both following lists. It verifies all 32 samples
on every audible bin, one pending trap payload with its unlink cursors, and the
second finished voice's later trap after simulated ISR cleanup. It also covers
an invalid idle cycle and pause/shutdown while a queued voice stays locked.
Neither cancellation path may process source samples.

The IRQ-dependent continuation fixture also compiles the actual core IRQ
publication code. A finished voice raises a trap while an ongoing voice is
locked; only the connected ISR can unlock it. The ISR must receive the paired
trap payload and cursors before the VP finishes its frame, and all continuing
voices must then consume exactly one complete tick. The old code misses a
one-second deadline; the test cancels its own worker so the negative control
fails without hanging the suite.

For a negative control, set `-DAPU_VP_TEST_SOURCE=<absolute baseline apu_vp.c>`.
Place that copied source in its own folder without a baseline `apu_state.h`;
the harness uses the current state header. The old raw-fetch implementation
must fail the rate/duration and lifecycle assertions. Its 24 kHz case produces
4099 outputs from 4099 source samples, while the fixed VP produces 8198.
