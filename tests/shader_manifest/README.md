# Vertex shader manifest regression

This Windows native test extracts the manifest section from the production `kelvin_gpu_vsh.inc` and runs it against a private synthetic cache directory. It does not compile shaders or read game files.

```powershell
cmake -S tests/shader_manifest -B build-shader-manifest -A x64
cmake --build build-shader-manifest --config Release
ctest --test-dir build-shader-manifest -C Release --output-on-failure
```

Set `-DBLACK_MANIFEST_SOURCE=<path>` to run the same regression against another source snapshot. The cases cover empty files, duplicate keys, partial headers and payloads both before first load and after the in-memory key set is initialized, corrupt headers, prefix preservation, and failed append followed by retry.
