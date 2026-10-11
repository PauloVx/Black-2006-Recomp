"""Build and run source-extracted native drop diagnostic checks (no game)."""
from pathlib import Path
import argparse
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", required=True, type=Path)
parser.add_argument("--cc", default="gcc")
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "tests/ambient_occlusion/camera"))
from extract_camera import function

source = (root / "src/recomp_manual.c").read_text()
start = source.index("/* BEGIN BLACK_WEAPON_TRACE")
end = source.index("/* END BLACK_WEAPON_TRACE */", start)
guard = function(source, "static int black_pc_ram_span(uint32_t address, uint32_t bytes)")
args.output.mkdir(parents=True, exist_ok=True)
(args.output / "trace.extracted.inc").write_text(guard + "\n" + source[start:end])
exe = args.output / "weapon-trace-test.exe"
subprocess.run([args.cc, "-O2", "-Wall", "-Wextra", "-frounding-math", "-fno-fast-math",
                "-I", str(args.output), str(Path(__file__).with_name("test_trace.c")), "-o", str(exe)], check=True)
for mode in ([], ["off"]):
    result = subprocess.run([str(exe), *mode], check=True, text=True, capture_output=True)
    records = [x for x in result.stderr.splitlines() if x.startswith("[WEAPON]")]
    assert len(records) == (0 if mode else 256), len(records)
    if not mode:
        assert any("xyz=7F800001" in x for x in records)
        assert any("slot=255" in x for x in records)
        assert any("entries=11111111,22222222,33333333" in x for x in records)
        assert any("slots=003FFFF6 slots_valid=0" in x for x in records)
    print(result.stdout.strip(), "disabled" if mode else "enabled")
