"""Extract the authored native definition so regressions exercise the shipping pages."""
import argparse
import re
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
text = args.source.read_text(encoding='utf-8')
match = re.search(r'black_menu_definition\[\]\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);', text)
if not match:
    raise SystemExit('could not find the production native menu definition')
args.output.write_text('static const char k_black_menu_default[] =\n' + match[1] + ';\n', encoding='utf-8')
