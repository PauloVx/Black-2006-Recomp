#!/usr/bin/env python3
"""Check the shipped movie subtitle matrix against the parser's UTF-16 limit."""

import argparse
import re
from pathlib import Path


LOCALES = ("", "pt-BR", "es", "fr", "de", "it")
STEMS = ("03_N", "05_N", "07_N", "09_N", "11_N", "13_N", "15_N", "17_N", "18_N")
TIME_LINE = re.compile(
    r"^\d{2}:\d{2}:\d{2},\d{3}\s+-->\s+\d{2}:\d{2}:\d{2},\d{3}(?:\s+.*)?$"
)
MAX_UTF16_UNITS = 255  # The 256-unit conversion buffer also needs its terminating NUL.


def expected_tracks(root: Path) -> dict[str, Path]:
    tracks = {}
    for locale in LOCALES:
        for stem in STEMS:
            relative = Path(locale) / f"{stem}.srt" if locale else Path(f"{stem}.srt")
            tracks[relative.as_posix()] = root / "subtitles" / relative
    return tracks


def read_cues(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8-sig")
    cues = []
    for number, block in enumerate(re.split(r"\r?\n\s*\r?\n", text.strip()), start=1):
        lines = block.splitlines()
        if len(lines) < 3 or not lines[0].strip().isdigit() or not TIME_LINE.fullmatch(lines[1].strip()):
            raise ValueError(f"{path}: malformed SRT cue block {number}")
        cue = "\n".join(line.rstrip() for line in lines[2:]).strip()
        if not cue:
            raise ValueError(f"{path}: empty SRT cue block {number}")
        cues.append(cue)
    if not cues:
        raise ValueError(f"{path}: no subtitle cues")
    return cues


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True, help="BLACK repository root")
    args = parser.parse_args()
    root = args.root.resolve()
    expected = expected_tracks(root)
    actual = {
        path.relative_to(root / "subtitles").as_posix()
        for path in (root / "subtitles").rglob("*.srt")
    }
    if actual != set(expected):
        missing = sorted(set(expected) - actual)
        unexpected = sorted(actual - set(expected))
        raise SystemExit(f"subtitle matrix mismatch: missing={missing}, unexpected={unexpected}")

    total_cues = 0
    longest = (0, "", 0)
    for relative, path in expected.items():
        for index, cue in enumerate(read_cues(path), start=1):
            units = len(cue.encode("utf-16-le")) // 2
            total_cues += 1
            if units > longest[0]:
                longest = (units, relative, index)
            if units > MAX_UTF16_UNITS:
                raise SystemExit(
                    f"{path}: cue {index} has {units} UTF-16 units; parser limit is {MAX_UTF16_UNITS}"
                )
    if longest[0] <= 160:
        raise SystemExit("the checked-in catalog no longer exercises cues longer than the old 160-unit limit")
    print(
        f"PASS: {len(expected)} tracks across 9 stems x 6 locales; {total_cues} cues; "
        f"longest is {longest[0]} UTF-16 units in {longest[1]} cue {longest[2]}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
