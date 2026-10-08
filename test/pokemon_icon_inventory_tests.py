"""Pinned physical inventory checks; no native compilation or GPU execution."""
import hashlib
import json
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from inventory_pokemon_icons import inventory
first = inventory()
second = inventory()
assert first == second, "Pinned icon inventory is not deterministic"
report = json.loads((ROOT / "docs/generated/POKEMON_ICON_SOURCE_INVENTORY.json").read_text(encoding="utf-8"))
assert report == first, "Published inventory differs from pinned source"
expected_hash = report.pop("contentSHA256")
assert hashlib.sha256(json.dumps(report, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")).hexdigest() == expected_hash
paths = [row["sourcePath"] for row in first["files"]]
assert paths == sorted(set(paths)), "Duplicate or unordered physical icons"
assert first["counts"]["physicalIcons"] == len(paths)
assert sum(first["counts"]["byRawAtlasDirectory"].values()) == len(paths)
assert all(row["mappingStatus"] == "REQUIRES_CANONICAL_APPEARANCE_RESOLUTION" for row in first["files"])
assert any(row["extensions"]["rawFrameKey"] == "1s" for row in first["files"])
print("PASS pinned physical icon inventory, provenance and two-run determinism:", len(paths))
