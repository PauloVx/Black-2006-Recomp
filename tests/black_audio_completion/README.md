This asset-free Windows fixture extracts the production HAL IRQL lock section
and BLACK's manual `sub_0021C7EF` wrapper plus `black_audio_queue_completion`
helper. A synthetic generated callee exposes primary-list removal and invokes
the actual completion helper. No retail bytes, generated
retail source, game launch, or frozen-process mutation is involved.

A controlled guest thread holds DISPATCH exclusion while a second thread runs
at device IRQL. Completion must return promptly, preserve the original busy
return's stack/register effects, and allow the enclosing ISR acknowledgement.
The voice remains linked, then completes once when tried after the owner exits.
The fixture also checks same-thread recursion, nested DPC/ISR ownership, device
IRQL preservation, and balanced release. Its old negative control removes only
the wrapper guard, retains the real membership helper, and must fail on mutation
during foreign DISPATCH ownership.

Membership tests execute the production helper on repeated completion before a
drain, a three-node list with repeated tail and middle entries, and fresh work
after draining. They check reciprocal links, stable ordering, one synthetic
callback per pending voice, and callbacks reading current synthetic voice state.
A separate old-membership control removes only the helper's membership condition
while preserving ISR exclusion. It reproduces the captured sentinel-to-node,
node-to-itself cycle and must fail the list and drain assertions. The synthetic
drain has a diagnostic iteration bound so this old control exits with failure.

The retry contract is supported by private inspection of the owned executable:
`0x0021C7EF` already exits when owner `+0x84` is busy, without unlinking the
voice; its only static caller is `0x0021CE08`, selected for trap method `0x8000`.
The enclosing `0x0021CE89` acknowledges/releases the trap after that call in
either case. The runtime's `apu_vp.c` frame walk traps each still-linked inactive
hardware voice again after trapped mode is released. These are addresses and
behavioral metadata only. The synthetic retry and drain do not execute those
retail routines. Native status changes, pending work flags and DPC notification
remain in the generated caller outside the replaced insertion block; this
fixture does not execute or validate that generated caller. It proves production
helper list coalescing and wrapper exclusion, not overall game stability or
prevention of every possible list corruption.

Configure this directory with CMake, build Release and Debug, and run CTest for
each configuration. `XBOXRECOMP_DIR` selects a patched runtime;
`BLACK_MANUAL_SOURCE` selects the corresponding production manual source.
