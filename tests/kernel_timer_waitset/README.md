The timer thread may wait on the device event, GPU event and high-resolution
timer together. Its production array must hold all three handles.

This asset-free fixture extracts and executes the actual wait branch in
`kernel_bridge.c` against deterministic host adapters. It covers all source
combinations, cached/failed timer creation, expired deadlines and timer-arm
failure. One extra allocated slot carries a boundary sentinel so the historical
two-slot declaration reproducibly corrupts that sentinel without writing beyond
the fixture allocation. The old-capacity executable is a required failing
negative control. No game launch or retail content is needed.

Configure this directory with CMake, build, and run CTest. Use
`-DXBOXRECOMP_DIR=<patched runtime>` to select another production source tree.
