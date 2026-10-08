"""Pinned constructor and physical platform alignment; no GPU execution."""
import hashlib
import json
import math
import re
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
audit = json.loads((ROOT / "docs/generated/ARENA_ALIGNMENT_AUDIT.json").read_text(encoding="utf-8"))
for source in audit["sources"]:
    path = ROOT / "build/upstream/pokerogue" / source["sourcePath"]
    assert hashlib.sha256(path.read_bytes()).hexdigest() == source["sha256"]
pokemon = (ROOT / "build/upstream/pokerogue/src/field/pokemon.ts").read_text(encoding="utf-8")
constructor = re.search(r"class PlayerPokemon\b[\s\S]*?super\((\d+),\s*(\d+),\s*species", pokemon)
assert constructor
x, y = map(int, constructor.groups())
expected = {"x": math.floor(x * 5 / 4 + 0.5), "y": math.floor(y * 5 / 4 + 0.5)}
assert audit["observations"]["nativePlayerAnchor"] == expected
layout = (ROOT / "project/include/runtime/DualScreenLayout.hpp").read_text(encoding="utf-8")
for axis in ("X", "Y"):
    match = re.search(r"kPlayerBattleAnchor" + axis + r"=(\d+)\.0f;", layout)
    assert match and int(match.group(1)) == expected[axis.lower()]
main = (ROOT / "project/src/main.cpp").read_text(encoding="utf-8")
assert "Pokerogue3DS::kPlayerBattleAnchorX, Pokerogue3DS::kPlayerBattleAnchorY, 0.0f, animationTimeMs)" in main
layers = json.loads((ROOT / "docs/generated/ARENA_LAYER_REPORT.json").read_text(encoding="utf-8"))
assert layers["revision"] == "056a1f408f26a3be4fef243f7462cb43608c7928"
for key, bounds in audit["observations"]["playerBaseAlphaBounds"].items():
    row = next(row for row in layers["files"] if row["key"] == key)
    path = ROOT / "build/native-presentation/source" / row["sourcePath"]
    assert hashlib.sha256(path.read_bytes()).hexdigest() == row["sourceSHA256"]
    with Image.open(path) as image:
        assert list(image.convert("RGBA").getchannel("A").getbbox()) == bounds
    center = (bounds[0] + bounds[2]) / 2 * 5 / 4
    assert abs(expected["x"] - center) < abs(audit["observations"]["previousNativePlayerAnchor"]["x"] - center)
assert audit["runtimeValidation"] == "NOT_EXECUTED"
print("PASS pinned player anchor and two physical platform bounds; GPU composition pending")
