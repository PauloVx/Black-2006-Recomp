"""Execute the production timer wait branch against deterministic host adapters.

Only its allocation gains one boundary sentinel. Cached timer state becomes
per-case state so creation/failure combinations can be exercised in one process.
No game image, generated guest code, or runtime libraries are needed.
"""
import pathlib
import re
import sys

source_path, harness_path, output_path, variant = sys.argv[1:]
source = pathlib.Path(source_path).read_text(encoding="utf-8")
thread_start = source.index("static DWORD WINAPI kernel_timer_thread")
start = source.index("        /* Device wake, GPU wake", thread_start)
end = source.index("        {\n            /* RECOMP_KERNEL_SLOWCALL", start)
body = source[start:end]
declaration = re.search(r"HANDLE wake\[(\d+)\];", body)
if declaration is None or body.count("WaitForMultipleObjects") != 2:
    raise RuntimeError("Timer wait branch changed; review the fixture extraction")
capacity = 2 if variant == "old" else int(declaration.group(1))
body = body.replace(declaration.group(0), f"""HANDLE wake_storage[{capacity} + 1] = {{0}};
        HANDLE *wake = wake_storage;
        wake_storage[{capacity}] = SENTINEL;""", 1)
body = body.replace("static HANDLE hrt;", "HANDLE hrt = scenario_cached_timer;")
body = body.replace("static int hrt_tried;", "int hrt_tried = scenario_timer_tried;")
body += f"\n    CHECK(wake_storage[{capacity}] == SENTINEL, \"wait-set boundary sentinel\");\n"
template = pathlib.Path(harness_path).read_text(encoding="utf-8")
if template.count("/* PRODUCTION_WAIT_BODY */") != 1:
    raise RuntimeError("Expected one production branch insertion point")
pathlib.Path(output_path).write_text(
    template.replace("/* PRODUCTION_WAIT_BODY */", body), encoding="utf-8"
)
