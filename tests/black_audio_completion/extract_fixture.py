"""Execute production IRQL locks, completion wrapper and list helper, asset-free.

Independent negative controls remove either the wrapper's nonblocking dispatch
guard or the list helper's membership condition. The synthetic generated callee
supplies observable primary-list effects and guest ABI results; it contains no
retail implementation or generated game code.
"""
import pathlib
import re
import sys

hal_path, manual_path, harness_path, output_path, variant = sys.argv[1:]
if variant not in ("current", "old", "old-membership"):
    raise ValueError("Expected current, old or old-membership variant")
hal = pathlib.Path(hal_path).read_text(encoding="utf-8")
manual = pathlib.Path(manual_path).read_text(encoding="utf-8")


def braced_end(source, opening):
    depth = 1
    i = opening + 1
    while depth:
        depth += (source[i] == "{") - (source[i] == "}")
        i += 1
    return i


start = hal.index("static XBOX_THREAD_LOCAL KIRQL g_current_irql")
end = hal.index("/* ============================================================================\n * KeTickCount", start)
irql = hal[start:end]
for api in ("xbox_IrqlTryEnterDispatch", "xbox_IrqlLeaveDispatch"):
    if api not in irql:
        raise RuntimeError(f"Production HAL lacks {api}")

start = manual.index("void sub_0021C7EF(void)\n{")
opening = manual.index("{", start)
wrapper = manual[start:braced_end(manual, opening)]
guard = re.search(r"if\s*\(\s*!xbox_IrqlTryEnterDispatch\(\)\s*\)\s*\{", wrapper)
if not guard or wrapper.count("xbox_IrqlLeaveDispatch();") != 1:
    raise RuntimeError("Completion guard changed; review negative control")
if "sub_0021C7EF_gen();" not in wrapper:
    raise RuntimeError("Wrapper must invoke the renamed generated callee")
if variant == "old":
    opening = wrapper.index("{", guard.start())
    wrapper = wrapper[:guard.start()] + wrapper[braced_end(wrapper, opening):]
    wrapper = wrapper.replace("xbox_IrqlLeaveDispatch();", "")

start = manual.index("uint32_t black_audio_queue_completion(uint32_t voice)\n{")
opening = manual.index("{", start)
helper = manual[start:braced_end(manual, opening)]
membership = re.search(
    r"if\s*\(GUEST32\(node\) == node && GUEST32\(node \+ 4\) == node\)\s*\{", helper
)
if not membership:
    raise RuntimeError("Completion membership condition changed; review old control")
if variant == "old-membership":
    opening = helper.index("{", membership.start())
    end = braced_end(helper, opening)
    helper = helper[:membership.start()] + helper[opening + 1:end - 1] + helper[end:]

template = pathlib.Path(harness_path).read_text(encoding="utf-8")
for marker in ("/* PRODUCTION_IRQL */", "/* PRODUCTION_WRAPPER */", "/* PRODUCTION_COMPLETION_HELPER */"):
    if template.count(marker) != 1:
        raise RuntimeError("Expected one production insertion point")
result = "#define FIXTURE_OLD_CONTROL " + str(int(variant == "old")) + "\n"
result += "#define FIXTURE_OLD_MEMBERSHIP_CONTROL " + str(int(variant == "old-membership")) + "\n" + template
result = result.replace("/* PRODUCTION_IRQL */", irql).replace("/* PRODUCTION_WRAPPER */", wrapper)
result = result.replace("/* PRODUCTION_COMPLETION_HELPER */", helper)
pathlib.Path(output_path).write_text(result, encoding="utf-8")
