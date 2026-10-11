"""Extract the authored constructor observer; validate bounded synthetic logs."""
from pathlib import Path
import subprocess
import sys

if sys.argv[1] == '--extract':
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'ambient_occlusion/camera'))
    from extract_camera import function
    source = Path(sys.argv[2]).read_text(encoding='utf-8')
    start = source.index('/* BEGIN BLACK_WEAPON_MODEL_TRACE')
    end = source.index('/* END BLACK_WEAPON_MODEL_TRACE */', start)
    guard = function(source, 'static int black_pc_ram_span(uint32_t address, uint32_t bytes)')
    Path(sys.argv[3]).write_text(guard + '\n' + source[start:end], encoding='utf-8')
elif sys.argv[1] == '--run':
    for mode in ([], ['off']):
        result = subprocess.run([sys.argv[2], *mode], check=True, text=True, capture_output=True)
        records = result.stderr.splitlines()
        if len(records) != (0 if mode else 128):
            raise RuntimeError(f'Unexpected model trace count: {len(records)}')
        if not mode:
            for part in ('name=123456787F800001', 'model_name=98765432ABCDEF01',
                         'slot=255', 'slots=003FFFF6 slots_valid=0', 'actor_valid=0',
                         'asset_valid=1', 'entry0=11111111'):
                if not any(part in line for line in records):
                    raise RuntimeError('Missing guarded observation: ' + part)
        print(result.stdout.strip(), 'disabled' if mode else 'enabled')
else:
    raise SystemExit('Expected --extract SOURCE OUTPUT or --run EXE')
