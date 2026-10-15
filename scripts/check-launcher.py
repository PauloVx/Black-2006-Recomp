"""Checks the launcher's hand-written C++ (launcher/src, launcher/tests) against launcher/.clang-format and,
when given a build folder with compile_commands.json (Ninja or Makefiles), launcher/.clang-tidy.

    python scripts/check-launcher.py                 # formatting only
    python scripts/check-launcher.py --fix           # reformat in place
    python scripts/check-launcher.py --tidy build-launcher-linux

clang-format and clang-tidy come from PATH, or: pip install clang-format==19.1.7 clang-tidy==19.1.0
"""
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAUNCHER = ROOT / 'launcher'


def sources():
    files = []
    for folder in ('src', 'tests'):
        files += sorted(p for p in (LAUNCHER / folder).rglob('*') if p.suffix in ('.cpp', '.h'))
    return files


def tool(name):
    found = shutil.which(name)
    if not found:
        sys.exit(f'{name} is not on PATH (pip install {name})')
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--fix', action='store_true', help='reformat files instead of reporting them')
    parser.add_argument('--tidy', metavar='BUILD', help='also run clang-tidy with BUILD/compile_commands.json')
    args = parser.parse_args()

    files = sources()
    clang_format = tool('clang-format')
    if args.fix:
        subprocess.run([clang_format, '-i', *map(str, files)], check=True)
        print(f'formatted {len(files)} files')
    else:
        result = subprocess.run([clang_format, '--dry-run', '-Werror', *map(str, files)])
        if result.returncode:
            sys.exit('formatting differs: run python scripts/check-launcher.py --fix')
        print(f'{len(files)} files formatted correctly')

    if args.tidy:
        database = Path(args.tidy) / 'compile_commands.json'
        if not database.is_file():
            sys.exit(f'{database} is missing: configure with the Ninja preset (linux-release) first')
        compiled = {Path(entry['file']).resolve() for entry in json.loads(database.read_text())}
        units = [f for f in files if f.suffix == '.cpp' and f.resolve() in compiled]
        result = subprocess.run([tool('clang-tidy'), '-p', str(Path(args.tidy)), '--quiet', *map(str, units)])
        if result.returncode:
            sys.exit('clang-tidy reported problems')
        print(f'clang-tidy: {len(units)} files clean')


if __name__ == '__main__':
    main()
