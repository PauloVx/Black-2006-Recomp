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
    render_start = source.index('/* BEGIN BLACK_WEAPON_RENDER_TRACE')
    render_end = source.index('/* END BLACK_WEAPON_RENDER_TRACE */', render_start)
    loadout_start = source.index('/* BEGIN BLACK_TEST_WEAPON_LOADOUT')
    loadout_end = source.index('/* END BLACK_TEST_WEAPON_LOADOUT */', loadout_start)
    unit_start = source.index('/* BEGIN BLACK_TEST_STREAM_UNIT')
    unit_end = source.index('/* END BLACK_TEST_STREAM_UNIT */', unit_start)
    guard = function(source, 'static int black_pc_ram_span(uint32_t address, uint32_t bytes)')
    Path(sys.argv[3]).write_text(guard + '\n' + source[start:end] + '\n' +
                               source[render_start:render_end] + '\n' +
                               source[loadout_start:loadout_end] + '\n' +
                               source[unit_start:unit_end], encoding='utf-8')
elif sys.argv[1] == '--run':
    for mode in ([], ['off'], ['wrong']):
        result = subprocess.run([sys.argv[2], *mode], check=True, text=True, capture_output=True)
        records = [line for line in result.stderr.splitlines() if line.startswith('[WEAPON]')]
        if len(records) != (0 if mode else 256):
            raise RuntimeError(f'Unexpected trace count: {len(records)}')
        attachments = [line for line in result.stderr.splitlines() if line.startswith('[WEAPON_ATTACHMENT]')]
        flashes = [line for line in result.stderr.splitlines() if line.startswith('[MUZZLE_GATE]')]
        loadouts = [line for line in result.stderr.splitlines() if line.startswith('[BLACK_TEST_WEAPON]')]
        if len(attachments) != (0 if mode else 128) or len(flashes) != (0 if mode else 64):
            raise RuntimeError(f'Unexpected attachment/flash counts: {len(attachments)}/{len(flashes)}')
        if len(loadouts) != (0 if mode else 8):
            raise RuntimeError(f'Unexpected lab loadout trace count: {len(loadouts)}')
        if not mode and ('accepted=0' not in loadouts[0] or 'accepted=1' not in loadouts[1]):
            raise RuntimeError('Missing fail-closed/native loadout observations')
        if not mode and ('stage=1 actor=00010000' not in attachments[41] or
                         'suppressed=1' not in attachments[41] or
                         'frame=255 variant=255' not in flashes[0] or
                         'size_bits=7F800001' not in flashes[1]):
            raise RuntimeError('Missing native state/invalid frame/raw size observations')
        if not mode and not (any('xyz=7F800001' in line for line in records) and
                             any('slot=255' in line for line in records) and
                             any('entries=11111111,22222222,33333333' in line for line in records) and
                             any('slots=003FFFF6 slots_valid=0' in line for line in records)):
            raise RuntimeError('Missing NaN/invalid slot observations')
        print(result.stdout.strip(), 'disabled' if mode else 'enabled')
else:
    raise SystemExit('Expected --extract SOURCE OUTPUT or --run EXE')
