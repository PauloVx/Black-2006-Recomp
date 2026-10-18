"""Extract the production controller wrapper so its guest-memory hooks can be tested natively."""

import argparse
from pathlib import Path


parser = argparse.ArgumentParser()
parser.add_argument("--source", required=True)
parser.add_argument("--output", required=True)
args = parser.parse_args()

source = Path(args.source).read_text(encoding="utf-8")
begin = "/* BEGIN BLACK_PC_CONTROLS"
end = "/* END BLACK_PC_CONTROLS */"
start = source.index(begin)
finish = source.index(end, start) + len(end)
Path(args.output).write_text(source[start:finish] + "\n", encoding="utf-8")
