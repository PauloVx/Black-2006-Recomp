"""Compile the actual memory getters without bringing in the full GPU runtime."""
import argparse
import re
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, required=True)
p.add_argument('--main', type=Path, required=True)
p.add_argument('--out', type=Path, required=True)
a = p.parse_args()
source = a.source.read_text(encoding='utf-8')
functions = []
for name in ('xbox_GetPhysicalMemoryBase', 'xbox_GetPhysicalMemorySize',
             'xbox_PhysicalAddressOf'):
    match = re.search(r'^\w[\w\s*]*\b' + name + r'\([^;]*?\)\s*\{',
                      source, re.MULTILINE)
    if not match:
        raise ValueError(f'Missing production function {name}')
    depth = 1
    end = match.end()
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    functions.append(source[match.start():end])
a.out.write_text('\n\n'.join(functions) + '\n', encoding='utf-8')
# Exercise the startup contract too: the device must be given the reported
# physical extent, rather than the standalone initializer's 64-MB default.
main = a.main.read_text(encoding='utf-8')
if not re.search(r'g_apu_state\s*=\s*mcpx_apu_init_standalone_sized\(\s*'
                 r'\(uint8_t\s*\*\)xbox_GetPhysicalMemoryBase\(\),\s*'
                 r'xbox_GetPhysicalMemorySize\(\)\s*\)', main):
    raise ValueError('APU startup does not use the mapped physical extent')
