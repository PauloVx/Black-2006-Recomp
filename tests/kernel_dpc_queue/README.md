Asset-free Windows regression for the actual production pending-DPC helpers,
guest bridge enqueue, timer enqueue and DPC drain in `kernel_bridge.c`.

A deterministic two-producer overlap pauses one producer immediately after it
reads the shared tail. The historical unlocked push lets another accepted GPU
completion token overwrite the same slot; the resumed first writer loses it.
The corrected queue retains both tokens without reaching capacity. The negative
control removes only the new producer lock from the extracted production code.

The corrected path additionally checks 20,000 entries across four concurrent
producers and a consumer, payload identity, exactly-once delivery, ring wrapping,
null/full bridge returns, IRQL deferral and callback requeue. A callback joins a
separate enqueueing thread to prove the queue lock is released before callbacks.

Configure this directory with CMake, build and run CTest. Both Release and Debug
include the required failing old-code control. No game launch or retail content
is needed.
