"""Extract production autofocus orchestration for a synthetic depth-gap regression."""
import argparse
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("--source", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
s = a.source.read_text()
start = s.index("static void kg_post_scene(KGpuSurf *cs, KGpuSurf *zs)\n{")
end = s.index("/* The target of a draw", start)
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(s[start:end])
