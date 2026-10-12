# APU physical backing regression

BLACK's low guest RAM has a second physical view after the 64-MB contiguous
arena. The APU previously received the standalone initializer's 64-MB default,
so DSP DMA bounds checks rejected pages and SGE tables in this mapped upper bank.
Startup now supplies the extent actually mapped by the runtime.

The test compiles the production physical-memory getters and calls production
DSP reset/MMIO bootstrap for GP and EP. It checks missing/adjacent/nonadjacent
views, overflow rejection, guest-to-physical translation, upper-bank SGE tables
and source pages, last-page bounds, 24-bit masking, and transactional rejection
when a source page is outside backing. Real GP/EP DMA descriptors also read and
write upper-bank PCM through the production scratch and FIFO SGE callbacks,
including native EP monitor capture and failed-transfer IRQ/cursor behavior.
It also verifies the startup call uses
the sized initializer. No retail assets or audio device are needed.

```powershell
cmake -S tests/apu_physical_memory -B build-apu-physical-memory -A x64
cmake --build build-apu-physical-memory --config Release --target apu-physical-memory-test
ctest --test-dir build-apu-physical-memory -C Release --output-on-failure
```

For a negative control, copy `xbox_memory_layout.c`, make its physical-size
getter report only the 64-MB arena, and pass the copy as
`-DMEMORY_LAYOUT_SOURCE=<absolute path>`. The upper-bank acceptance checks must
fail.

This is a verified memory contract correction. The issue reporter's intermittent
sound and freeze still require a representative runtime trace to link them to
this path.
