"""Inventory every physical icon in the pinned source without guessing actor identities."""
import hashlib
import io
import json
import struct
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSET_REVISION = "056a1f408f26a3be4fef243f7462cb43608c7928"
GAME_REVISION = "8555c08c823b856cbec4eb99ca84ea52a955836d"


def inventory(root=ROOT):
    assets = root / "build/upstream/pokerogue-assets"
    game = root / "build/upstream/pokerogue"
    source_path = "src/data/pokemon-species.ts"
    source = subprocess.check_output(["git", "-C", str(game), "show", GAME_REVISION + ":" + source_path])
    archive = subprocess.check_output(["git", "-C", str(assets), "archive", "--format=zip", ASSET_REVISION, "images/pokemon/icons"])
    records = []
    with zipfile.ZipFile(io.BytesIO(archive)) as files:
        for name in sorted(files.namelist()):
            if not name.endswith(".png"):
                continue
            raw = files.read(name)
            if raw[:8] != b"\x89PNG\r\n\x1a\n" or raw[12:16] != b"IHDR" or len(raw) < 33:
                raise ValueError("Invalid PNG header: " + name)
            width, height = struct.unpack(">II", raw[16:24])
            if not width or not height:
                raise ValueError("Empty PNG: " + name)
            path = Path(name)
            records.append({"sourcePath": name, "sourceSHA256": hashlib.sha256(raw).hexdigest(),
                "width": width, "height": height, "extensions": {"rawAtlasDirectory": path.parent.name,
                "rawFrameKey": path.stem}, "mappingStatus": "REQUIRES_CANONICAL_APPEARANCE_RESOLUTION"})
    if not records:
        raise ValueError("Pinned icon archive contains no PNGs")
    directories = {}
    for row in records:
        key = row["extensions"]["rawAtlasDirectory"]
        directories[key] = directories.get(key, 0) + 1
    content = {"schemaVersion": 1, "repository": "https://github.com/pagefaultgames/pokerogue-assets",
        "revision": ASSET_REVISION, "sourceRoot": "images/pokemon/icons", "files": records,
        "counts": {"physicalIcons": len(records), "byRawAtlasDirectory": directories},
        "identityReference": {"repository": "https://github.com/pagefaultgames/pokerogue",
        "revision": GAME_REVISION, "sourcePath": source_path,
        "sourceSHA256": hashlib.sha256(source).hexdigest(),
        "sourceSymbols": ["PokemonSpecies.getIconId", "PokemonSpecies.getIconAtlasKey"]},
        "limitations": ["Inventory preserves raw frame keys; it does not claim canonical appearance mapping or runtime rendering.",
            "Event replacements and variantData must be resolved before binding icons to actors."]}
    content["contentSHA256"] = hashlib.sha256(json.dumps(content, ensure_ascii=False, sort_keys=True,
        separators=(",", ":")).encode("utf-8")).hexdigest()
    return content


if __name__ == "__main__":
    result = inventory()
    target = ROOT / "docs/generated/POKEMON_ICON_SOURCE_INVENTORY.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes((json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8"))
    print(json.dumps({"counts": result["counts"], "contentSHA256": result["contentSHA256"]}, sort_keys=True))
