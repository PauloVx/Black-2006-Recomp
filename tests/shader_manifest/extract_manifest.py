"""Extract only the production manifest implementation for native file-I/O regression tests."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--source", required=True)
parser.add_argument("--output", required=True)
args = parser.parse_args()

source = Path(args.source).read_text(encoding="utf-8")
start_marker = "/* ---- the manifest of the programs met so far ---- */"
end_marker = "/* ---- compiling ---- */"
start = source.index(start_marker)
end = source.index(end_marker, start)
if end <= start:
    raise SystemExit("manifest section markers are out of order")
Path(args.output).write_text(source[start:end], encoding="utf-8")
