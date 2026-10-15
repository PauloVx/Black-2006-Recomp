"""Checks the launcher's translations (launcher/assets/translations/<locale>.json) against its text.

The text is every string the launcher passes to tr(), trf() or trMark() in launcher/src/app, and the action and key
names of launcher/src/core/InputBindings.cpp. Each catalog must translate all of it, keep trf()'s {0}, {1}...
placeholders, and hold nothing else.

    python scripts/check-launcher-translations.py           # report problems (exit code 1 if any)
    python scripts/check-launcher-translations.py --update  # add new text (empty) and drop unused entries
"""
import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LAUNCHER = ROOT / 'launcher'
CATALOGS = LAUNCHER / 'assets' / 'translations'
LOCALES = ('pt-BR', 'es', 'fr', 'de', 'it')

LITERALS = r'"(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*'
CALL = re.compile(r'\b(?:tr|trf|trMark)\(\s*(' + LITERALS + ')')
ACTION = re.compile(r'Action\{"\w+", "((?:[^"\\]|\\.)*)"')
KEY = re.compile(r'Key\{"\w+", -?0?x?[0-9A-Fa-f]+, "((?:[^"\\]|\\.)*)"\}')
PLACEHOLDER = re.compile(r'\{\d+\}')


def unquote(run: str) -> str:
    text = ''.join(re.findall(r'"((?:[^"\\]|\\.)*)"', run))
    return re.sub(r'\\u([0-9A-Fa-f]{4})|\\(.)', lambda m: chr(int(m[1], 16)) if m[1] else {'n': '\n'}.get(m[2], m[2]),
                  text)


def source_text():
    found = []
    for path in sorted((LAUNCHER / 'src' / 'app').glob('*.cpp')):
        found += [unquote(m[1]) for m in CALL.finditer(path.read_text(encoding='utf-8'))]
    bindings = (LAUNCHER / 'src' / 'core' / 'InputBindings.cpp').read_text(encoding='utf-8')
    found += [unquote('"' + m[1] + '"').upper() for m in ACTION.finditer(bindings)]
    found += [unquote('"' + m[1] + '"') for m in KEY.finditer(bindings) if re.search('[a-z]', m[1])]  # not "A", "F1"
    unique = []
    for text in found:
        if text not in unique:
            unique.append(text)
    return unique


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--update', action='store_true', help='add missing text and remove unused entries')
    args = parser.parse_args()

    text = source_text()
    problems = 0
    for locale in LOCALES:
        path = CATALOGS / f'{locale}.json'
        catalog = json.loads(path.read_text(encoding='utf-8')) if path.exists() else {}
        missing = [t for t in text if not catalog.get(t)]
        unused = [t for t in catalog if t not in text]
        mismatched = [t for t in text if catalog.get(t) and
                      sorted(PLACEHOLDER.findall(t)) != sorted(PLACEHOLDER.findall(catalog[t]))]
        if args.update:
            updated = {t: catalog.get(t, '') for t in text}
            path.write_text(json.dumps(updated, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        for label, entries in (('missing', missing), ('unused', unused), ('placeholders differ', mismatched)):
            if entries and not (args.update and label == 'unused'):
                problems += len(entries)
                print(f'{locale}: {len(entries)} {label}')
                for entry in entries[:10]:
                    print(f'    {entry!r}')
    print(f'{len(text)} strings, {len(LOCALES)} languages: ' + ('all translated' if not problems else f'{problems} problems'))
    sys.exit(1 if problems and not args.update else 0)


if __name__ == '__main__':
    main()
