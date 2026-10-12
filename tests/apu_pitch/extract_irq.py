"""Compile the production core IRQ publication with the isolated VP fixture."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, required=True)
parser.add_argument("--out", type=Path, required=True)
args = parser.parse_args()
source = args.source.read_text()
start = source.index("static void (*s_irq_line)(int asserted);")
end = source.index("\n/* ============================================================", start)
args.out.write_text(source[start:end])
