# Windows audio output regression

The extractor takes the complete XAudio2 backend and APU frame-thread routine
from the patched runtime. A fake audio graph implements the device's buffer
ownership contract, allowing checks without an audio device or game assets.

Coverage includes two-packet startup, one successful start, refusal to overwrite
three queued packets, buffer reuse after consumption, submit failure, invalid
input, retry after a failed voice start, and restoration of two-packet buffering
after an empty queue. Stop failure also preserves the running voice. The test
checks retained sample contents as well as counters and queue order.

Critical-error tests inject callbacks while three packets are owned, check
teardown before buffer reuse, prime two fresh packets, and cover partial graph
creation failures and one-second retry limits. Healthy full or silent queues
do not recreate the graph. Playback statistics remain cumulative across graph
replacement. COM initialization, submission and shutdown share the APU thread;
successful COM calls are balanced, including `S_FALSE`.

The lifecycle fixture executes the actual frame thread through initialization,
idle signaling and finalization. Windows builds also compile the complete
unmodified backend against the real SDK. The recovery follows Microsoft's
[critical-error guidance](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2enginecallback-oncriticalerror)
and [COM initialization contract](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex).

```powershell
cmake -S tests/apu_output -B build-apu-output -A x64
cmake --build build-apu-output --config Release
ctest --test-dir build-apu-output -C Release --output-on-failure
```

Use `-DXBOXRECOMP_DIR=<patched-runtime>` for an alternate runtime checkout.
The audio CI workflow runs Release and Debug configurations. Actual WASAPI
recordings are separate gameplay evidence; they are not included in the source
repository.
