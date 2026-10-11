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
effect/submix inputs. Native EP output is still used whenever it is available.
This is a dry stereo fallback, not an emulation of the guest's effect graph or
headphone HRTF. Headphone front/back localization and long-run synchronization
still need gameplay recordings to validate.

The test links the production APU and calls `mcpx_apu_dsp_frame`. No game data
or audio device is needed. Independent signed impulses test each of the 32
buses at every epoch offset. Additional cases test one-sided sounds with loud
effect sends, unclipped dry gain, chronological stereo blocks, saturation,
native PCM ownership and VP diagnostic ownership. A separate process retains
the `RECOMP_APU_MIXDOWN_ALL=0` front-only diagnostic control.

```powershell
cmake -S tests/apu_spatial -B build-apu-spatial -G "Visual Studio 17 2022" -A x64
cmake --build build-apu-spatial --config Release
ctest --test-dir build-apu-spatial -C Release --output-on-failure
```

Use `-DXBOXRECOMP_DIR=<patched runtime>` and
`-DDSP56300_SOURCE_DIR=<pinned DSP checkout>` for a separate checkout. Set
`-DAPU_DSP_TEST_SOURCE=<prior apu_dsp.c>` to compile the original DSP into the
test executable as a negative control. The old parity fallback must fail the
spatial tests while the front-only control continues to pass.

Speaker bus IDs are independently documented in the public
[Cxbx DirectSound types](https://github.com/Cxbx-Reloaded/Cxbx-Reloaded/blob/master/src/core/hle/DSOUND/XbDSoundTypes.h).
The runtime's previous 3D trace identifies the paired 6/7 and 8/9 dry buses.
