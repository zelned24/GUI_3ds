"""Re-run asset conversion and compare every report/header/physical output byte hash."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def snapshot():
    paths=[root / "docs/generated/ARENA_BACKGROUND_REPORT.json",root / "docs/generated/ARENA_LAYER_REPORT.json",
        root / "project/generated/include/content/ArenaTextures.hpp",root / "project/generated/include/content/ArenaLayerTextures.hpp"]
    for report_path in paths[:2]:
        report=json.loads(report_path.read_text())
        for row in report["files"]:
            for key in ("runtimePath","titleRuntimePath","metadataPath"):
                if row.get(key):paths.append(root / "build/romfs" / row[key].removeprefix("romfs:/"))
    return {str(path.relative_to(root)):hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
before=snapshot()
for script in ("prepare_arena_backgrounds.py","prepare_arena_layers.py"):
    subprocess.run([sys.executable,str(root / "scripts" / script)],cwd=root,check=True)
after=snapshot()
assert before==after, {path:(before.get(path),after.get(path)) for path in before.keys()|after.keys() if before.get(path)!=after.get(path)}
print(f"PASS deterministic arena conversion: {len(after)} reports, headers, textures and metadata")
