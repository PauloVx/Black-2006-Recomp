"""Extract authored first-person projection code; no translated game code."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "tests/ambient_occlusion/camera"))
from extract_camera import function

source = (root / "src/recomp_manual.c").read_text(encoding="utf-8-sig")
start = source.index("/* BEGIN BLACK_VIEWMODEL_OWNER")
end = source.index("/* END BLACK_VIEWMODEL_OWNER */", start)
guard = function(source, "static int black_pc_ram_span(uint32_t address, uint32_t bytes)")
Path(sys.argv[1]).write_text(guard + "\n" + source[start:end], encoding="utf-8")
