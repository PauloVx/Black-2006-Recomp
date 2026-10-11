# Extracting BLACK Xbox speech and streamed audio

Use `scripts/extract_audio.py` with your own original Xbox assets and the
[official vgmstream command-line release](https://github.com/vgmstream/vgmstream/releases).
Keep the decoder's supplied DLLs beside its executable. The tool does not modify
the banks or require launching the game.

```powershell
python scripts/extract_audio.py game/levels/level_00/spch_EN.ssh --list
python scripts/extract_audio.py game/levels/level_00/spch_EN.ssh --all --output audio/level_00 --vgmstream C:/Tools/vgmstream/vgmstream-cli.exe
python scripts/extract_audio.py game/sound/streams/7thWave.ssh --entry 1 --output audio/7thWave --vgmstream C:/Tools/vgmstream/vgmstream-cli.exe
```

Each `.ssh` header must remain beside its matching `.bks` payload. Entry numbers
are one-based; repeat `--entry` to select several clips. WAV files preserve the
recorded sample rate and channel count, and a JSON manifest records the bank
offsets and clip durations. Choose a fresh output directory for another export;
the tool refuses to overwrite earlier results.

The Xbox files checked here use **EA XAS v0**, not PCM, PS2 ADPCM, or Xbox IMA.
Each 16 KiB channel block contains 862 compressed frames plus six padding bytes.
Those padding bytes must be removed at every block boundary. Stereo channel
blocks must also be separated before decoding. A raw import with the wrong
codec, interleave, padding, or sample rate can produce distortion or wrong pitch.

The extractor validates codec, bounds, alignment and frame headers, then uses
vgmstream's existing XAS decoder through temporary mono GIN wrappers. The
[vgmstream GIN parser](https://github.com/vgmstream/vgmstream/blob/r2117/src/meta/gin.c)
and [XAS decoder](https://github.com/vgmstream/vgmstream/blob/r2117/src/coding/ea_xas_decoder.c)
document that decoding route. No decoder source or retail audio is bundled.

Validation covered the metadata of 43 stock headers containing 4,163 entries,
selected mono speech and stereo exports, and an independent synthetic PCM oracle
across block boundaries with vgmstream r2117. The test banks are generated locally
and contain no game audio.

This supports the checked Xbox SSH/BKS format. Headerless BKS files, AWD sound
banks, PS2 assets and movie audio require their own readers. Durations include
the game's end padding. Clip indices are bank identities, not subtitle timings;
the script does not infer dialogue text or build an in-game subtitle system.
