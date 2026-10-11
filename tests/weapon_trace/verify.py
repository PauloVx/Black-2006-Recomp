"""Extract authored helper or validate its production-shaped fixture output."""
from pathlib import Path
import subprocess
import sys

if sys.argv[1] == '--extract':
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'ambient_occlusion/camera'))
    from extract_camera import function
    source = Path(sys.argv[2]).read_text(encoding='utf-8')
    start = source.index('/* BEGIN BLACK_WEAPON_TRACE')
    end = source.index('/* END BLACK_WEAPON_TRACE */', start)
    guard = function(source, 'static int black_pc_ram_span(uint32_t address, uint32_t bytes)')
    Path(sys.argv[3]).write_text(guard + '\n' + source[start:end], encoding='utf-8')
elif sys.argv[1] == '--run':
    for mode in ([], ['off']):
        result = subprocess.run([sys.argv[2], *mode], check=True, text=True, capture_output=True)
        records = [line for line in result.stderr.splitlines() if line.startswith('[WEAPON]')]
        if len(records) != (0 if mode else 256):
            raise RuntimeError(f'Unexpected trace count: {len(records)}')
        if not mode and not (any('xyz=7F800001' in line for line in records) and
                             any('slot=255' in line for line in records) and
                             any('entries=11111111,22222222,33333333' in line for line in records) and
                             any('slots=003FFFF6 slots_valid=0' in line for line in records)):
            raise RuntimeError('Missing NaN/invalid slot observations')
        print(result.stdout.strip(), 'disabled' if mode else 'enabled')
else:
    raise SystemExit('Expected --extract SOURCE OUTPUT or --run EXE')
