"""Check locally generated unsigned CRT helpers and the weapon-name call chain.

Requires locally generated game sources. No translated game code is distributed
by this test; extracted functions and binaries stay in the requested build dir.
"""
from pathlib import Path
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--generated', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--cc', default='gcc')
parser.add_argument('--expect-broken', action='store_true')
parser.add_argument('--save-compat', action='store_true')
args = parser.parse_args()
symbols = ('sub_000A30B0', 'sub_000A3380', 'sub_000C09F0')
functions = {}
for path in sorted(args.generated.glob('*.c')):
    source = path.read_text(encoding='utf-8')
    for symbol in symbols:
        match = re.search(r'^void ' + symbol + r'\(void\)\n\{.*?^\}', source, re.M | re.S)
        if match:
            if symbol in functions:
                raise ValueError('Duplicate generated helper: ' + symbol)
            functions[symbol] = match[0]
if set(functions) != set(symbols):
    raise ValueError('Missing local generated helpers: ' + str(set(symbols)-set(functions)))
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'math.extracted.inc').write_text('\n\n'.join(functions[s] for s in symbols))
exe = args.output / 'weapon-model-math.exe'
subprocess.run([args.cc, '-O2', '-fwrapv', '-I', str(args.output),
    str(Path(__file__).with_name('test_math.c')), '-o', str(exe)], check=True)
subprocess.run([str(exe), *(['save-compat'] if args.save_compat else
                          ['broken'] if args.expect_broken else [])], check=True)
