"""Extract the actual AO orchestration function; the C fixture supplies fake GPU boundaries."""
import argparse
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("--source", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
s = a.source.read_text()
start = s.index("static void kg_post_ao_scene(KGpuSurf *cs, KGpuSurf *zs)\n{")
end = s.index("static void kg_post_scene(KGpuSurf *cs, KGpuSurf *zs)", start)
a.output.parent.mkdir(parents=True, exist_ok=True)
draw_start = s.index("static inline bool post_main_target(const KGpuSurf *cs)")
draw_end = s.index("/* A change in the settings:", draw_start)
a.output.write_text(s[start:end] + "\n" + s[draw_start:draw_end])
