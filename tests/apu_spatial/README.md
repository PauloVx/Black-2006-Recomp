# APU spatial fallback regression

The prior fallback sent every even mixbin left and every odd mixbin right.
That put center audio solely on the left and bass solely on the right. It also
treated DSP reverb/effect sends as additional dry speakers, so a source on the
right with a loud reverb send could sound louder on the left and clip.
BLACK also uses a stereo bus assignment of `[0,1,11,12]`: left samples go to
0 and 11, and right samples to 1 and 12. The old parity rule played each effect
send on the opposite side, collapsing the stereo image and doubling equal
left/right signals before clipping.

The fallback now preserves the front/back left and right speaker and 3D buses,
folds center and LFE equally into stereo at -3 dB per channel, and excludes raw
effect/submix inputs unless the title explicitly configures a primary stereo
pair. BLACK configures 19/20: captured PCM voices route exclusively to that
pair, and its native audio setup assigns it two channels. Without that pair,
cutscene soundtracks and some level sounds never reach the fallback output.
The auxiliary 11/12 and 21..31 sends remain excluded. Native EP output is still
used whenever it is available.
This is a dry stereo fallback, not an emulation of the guest's effect graph or
headphone HRTF. Headphone front/back localization and long-run synchronization
still need gameplay recordings to validate.

The complete dry stereo sum now uses a stereo-linked peak limiter before its
first 16-bit conversion. Individually valid voices can overlap above full scale;
independent hard clamps flattened their peaks and changed the left/right ratio.
The limiter retains unity gain until an overload, reduces both channels together
immediately, and releases with a 100 ms time constant at 48 kHz. State continues
across 32-sample ticks and silence, resets on DSP reset and native EP/VP bypass,
and adds no buffering or latency. The final clamp remains as a rounding guard.
This prevents this fallback's hard saturation; instantaneous gain changes and
the omitted guest effect graph still limit fidelity. The separate software
DirectSound mixer is outside this correction.

With `RECOMP_APU_TRACE_OUTPUT=1`, `overloaded` counts raw channel samples outside
full scale, `limited` counts nonzero channel samples attenuated by the gain
envelope, and `clipped` counts only actual post-limit hard saturation. The
`mixer-active` field identifies whether the separate software mixer is in use.

The test links the production APU and calls `mcpx_apu_dsp_frame`. No game data
or audio device is needed. Independent signed impulses test each of the 32
buses at every epoch offset. Additional cases test one-sided sounds with loud
effect sends, unclipped dry gain, chronological stereo blocks, peak limiting,
native PCM ownership and VP diagnostic ownership. Configured-pair cases check
signed stereo, every epoch offset, coexistence with standard speakers,
auxiliary-send exclusion, arbitrary channel ordering, and invalid requests.
Overlapping-source cases check the quieter channel's preserved ratio, release
through silence and frame boundaries, DSP reset, finite input handling, and
separate overload/limiting/hard-clipping counters. A sustained 1 kHz overlapped
tone checks stereo ratio, absence of hard-clipping plateaus, and the residual
after projecting onto the fundamental (less than 0.3% full scale).
A separate process retains
the `RECOMP_APU_MIXDOWN_ALL=0` front-only diagnostic control.

```powershell
cmake -S tests/apu_spatial -B build-apu-spatial -G "Visual Studio 17 2022" -A x64
cmake --build build-apu-spatial --config Release
ctest --test-dir build-apu-spatial -C Release --output-on-failure
```

Use `-DXBOXRECOMP_DIR=<patched runtime>` and
`-DDSP56300_SOURCE_DIR=<pinned DSP checkout>` for a separate checkout. Set
`-DAPU_DSP_TEST_SOURCE=<alternate apu_dsp.c>` to compile a negative control.
The alternate source must supply the current public DSP API, including
`mcpx_apu_set_stereo_fallback_pair`: an unmodified older source cannot link.
To isolate the custom-pair fix, copy the current source and remove just the
configured-pair additions to left/right output. Its custom-bus cases must fail,
while the front-only control continues to pass. An older parity source can
instead supply a validated setter stub and must fail the broader spatial cases.

For a limiter negative control, set `APU_DSP_TEST_SOURCE` to the pre-limiter
atomic-pair source with the same public API and current state header. Its dry
routing checks pass, while overlap ratio, frame continuity, silence release,
plateau, harmonic-residual and limiter telemetry checks must fail.

Speaker bus IDs are independently documented in the public
[Cxbx DirectSound types](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/master/src/core/hle/DSOUND/XbDSoundTypes.h).
The runtime's previous 3D trace identifies the paired 6/7 and 8/9 dry buses.
