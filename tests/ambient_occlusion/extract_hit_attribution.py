"""Extract the production hit history and lookup for deterministic ownership regressions."""
import argparse
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
text = args.source.read_text(encoding='utf-8')
history = re.search(r'#define BLACK_HITS .*?static unsigned black_hit_next;', text, re.S)
lookup = re.search(r'static int black_hit_by\(uint32_t soldier\)\n\{.*?\n\}', text, re.S)
if not history or not lookup:
    raise SystemExit('could not find the production hit history and lookup')
args.output.write_text(history[0] + '\n\n' + lookup[0] + '\n', encoding='utf-8')
