# Windows audio output regression

The extractor takes the complete XAudio2 backend, APU frame-thread routine,
and monitor packet submission call site and retry helper
from the patched runtime. A fake audio graph implements the device's buffer
ownership contract, allowing checks without an audio device or game assets.

Coverage includes two-packet startup, one successful start, refusal to overwrite
three queued packets, buffer reuse after consumption, submit failure, invalid
input, retry after a failed voice start, and restoration of two-packet buffering
after an empty queue. Stop failure also preserves the running voice. The test
checks retained sample contents as well as counters and queue order.

Healthy queue backpressure preserves the next complete packet. The production
monitor waits in at most 2 ms slices that release the APU lock, with a 64 ms
total deadline. Tests consume a buffer after 22 ms and verify exact packet
order, then exercise the timeout, guest pause, shutdown, and device loss during
the wait. They verify the lock is available during every wait. Device errors
keep their recovery/discard behavior; a failed Start after successful Submit
still means accepted PCM, so retrying cannot duplicate a queued packet.

`backpressure-packets` counts packets that needed at least one wait,
`backpressure-timeouts` counts the exceptional 64 ms deadline, and
`producer-dropped` counts source samples discarded on timeout, interruption,
or error. `queue-full` counts backend refusal attempts, including safe retries.
These corrections do not establish the cause of a particular recorded crackle.

Real VP output also waits when the guest front end is off, halted, or trapped.
It releases the APU lock for the guest ISR and preserves the partly filled EP
epoch instead of advancing zero slices. The actual frame-thread fixture checks
trap clearing, OFF/HALT gating, pause/shutdown wakeups, and control changes
during throttle. Explicit software mixer/test tone output still runs. The
`gated-waits` counter is enabled by output tracing and records these waits.

Finite software-mixer EOF coverage runs the extracted production renderer and
frame thread with a 100-sample voice. It checks that the final partial packet is
zero-padded and primed only after native production gates, then the frame thread
returns to its bounded idle wait without running VP work. Additional cases cover
an exact 1024-sample voice, a data-bearing native partial after the previous
queue drains, one bounded Start retry, pause/shutdown during backpressure, and
tail reset when both audio backends are unavailable. Against an older core
source, the EOF case confirms the old loop leaves 256 samples buffered and
submits none; helper-only tail cases print `SKIP` when those helpers are absent.

Critical-error tests inject callbacks while three packets are owned, check
teardown before buffer reuse, prime two fresh packets, and cover partial graph
creation failures and one-second retry limits. Healthy full or silent queues
do not recreate the graph. Playback statistics remain cumulative across graph
replacement. COM initialization, submission and shutdown share the APU thread;
successful COM calls are balanced, including `S_FALSE`.
The owner thread also registers with the optional MMCSS `Pro Audio` task and
reverts at finalization. Fake tests cover unavailable DLL, missing exports,
registration failure, successful ownership, and rejection of another thread's
shutdown. Registration follows the [calling-thread API contract](https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avsetmmthreadcharacteristicsw)
and [same-thread revert requirement](https://learn.microsoft.com/en-us/windows/win32/api/avrt/nf-avrt-avrevertmmthreadcharacteristics).

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
For a source negative control, set `-DAPU_OUTPUT_BACKEND_SOURCE=<old-apu_xaudio2.c>`
and `-DAPU_OUTPUT_CORE_SOURCE=<old-apu_core.c>` together. The extractor retains
the older one-attempt monitor call site; unchanged graph/ownership checks still
run, while chronology, acceptance status, and MMCSS checks reject that source.
The audio CI workflow runs Release and Debug configurations. Actual WASAPI
recordings are separate gameplay evidence; they are not included in the source
repository.
