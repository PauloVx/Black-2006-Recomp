"""Compile the production queue, bridge enqueue, timer enqueue and drain.

An overlap hook after the producer reads tail deterministically recreates the
old lost-write interleaving. The negative control removes only the new push
lock, retaining the production storage/payload/full-return operations.
"""
import pathlib
import sys

source_path, harness_path, output_path, variant = sys.argv[1:]
source = pathlib.Path(source_path).read_text(encoding="utf-8")


def function(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    i = opening + 1
    while depth:
        depth += (source[i] == "{") - (source[i] == "}")
        i += 1
    return source[start:i]


start = source.index("#define XBOX_MAX_PENDING_DPC")
end = source.index("/* Call a connected interrupt service routine.", start)
queue = source[start:end]
push_start = queue.index("static int kernel_dpc_push(")
push_end = queue.index("static int kernel_dpc_pop(", push_start)
push = queue[push_start:push_end]
if push.count("tail = g_dpc_tail;") != 1:
    raise RuntimeError("Producer changed; review overlap hook")
if variant == "old":
    push = push.replace(
        "    InitOnceExecuteOnce(&g_dpc_queue_once, kernel_dpc_queue_init, NULL, NULL);\n", ""
    ).replace("    EnterCriticalSection(&g_dpc_queue_cs);\n", "").replace(
        "    LeaveCriticalSection(&g_dpc_queue_cs);\n", ""
    )
push = push.replace("    tail = g_dpc_tail;", "    tail = g_dpc_tail;\n    fixture_overlap_point();")
queue = queue[:push_start] + push + queue[push_end:]
drain = function("static void kernel_drain_dpcs(void)\n{")
timer_enqueue = function("static void kernel_queue_dpc(")
template = pathlib.Path(harness_path).read_text(encoding="utf-8")
for marker in ["/* PRODUCTION_QUEUE */", "/* PRODUCTION_DRAIN */"]:
    if template.count(marker) != 1:
        raise RuntimeError("Expected one insertion point per production region")
result = "#define FIXTURE_OLD_CONTROL " + str(int(variant == "old")) + "\n" + template.replace("/* PRODUCTION_QUEUE */", queue).replace(
    "/* PRODUCTION_DRAIN */", drain + "\n\n" + timer_enqueue
)
pathlib.Path(output_path).write_text(result, encoding="utf-8")
