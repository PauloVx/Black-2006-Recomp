"""Extract the production runtime mode helpers without requiring game data."""

from pathlib import Path
import sys


source = Path(sys.argv[1]).read_text(encoding="utf-8")
begin = "/* BEGIN BLACK_WEAPON_RUNTIME_MODES */"
end = "/* END BLACK_WEAPON_RUNTIME_MODES */"
if source.count(begin) != 1 or source.count(end) != 1:
    raise ValueError("Missing or duplicate weapon runtime mode boundary")
start = source.index(begin) + len(begin)
stop = source.index(end)
if stop <= start:
    raise ValueError("Reversed weapon runtime mode boundary")
Path(sys.argv[2]).write_text(source[start:stop], encoding="utf-8")
