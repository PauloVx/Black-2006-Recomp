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
args.output.mkdir(parents=True, exist_ok=True)
subprocess.run([sys.executable, str(Path(__file__).with_name("verify.py")), "--extract",
                str(root / "src/recomp_manual.c"), str(args.output / "trace.extracted.inc")], check=True)
exe = args.output / "weapon-trace-test.exe"
subprocess.run([args.cc, "-O2", "-Wall", "-Wextra", "-frounding-math", "-fno-fast-math",
                "-I", str(args.output), str(Path(__file__).with_name("test_trace.c")), "-o", str(exe)], check=True)
subprocess.run([sys.executable, str(Path(__file__).with_name("verify.py")), "--run", str(exe)], check=True)
