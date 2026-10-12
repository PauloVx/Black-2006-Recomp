"""Extract the authored scope policy; no translated game code or retail assets."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ambient_occlusion/camera"))
from extract_camera import function

source = Path(sys.argv[1]).read_text(encoding="utf-8")
start = source.index("/* BEGIN BLACK_PC_SCOPE_MASK")
end = source.index("/* END BLACK_PC_SCOPE_MASK */", start)
helpers = [function(source, signature) for signature in (
    "static int black_pc_ram_span(uint32_t address, uint32_t bytes)",
    "static int black_hud_span(uint32_t address, uint32_t bytes)",
    "static int black_hud_disjoint(uint32_t a, uint32_t an,\n                              uint32_t b, uint32_t bn)",
    "static uint32_t black_hud_scale_bits(void)",
    "static __m128 black_hud_scalar(uint32_t bits)",
    "static uint32_t black_hud_result(__m128 value)")]
Path(sys.argv[2]).write_text("\n".join(helpers) + "\n" + source[start:end], encoding="utf-8")
