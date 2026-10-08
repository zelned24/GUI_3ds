"""Materialize pinned shader palettes as PNG assets; never compile the program."""
import argparse
import hashlib
import io
import json
import re
import subprocess
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]

def apply_palette(image, palette):
    # sprite-frag-shader.frag: first matching RGB, alpha > 0, 32 uniforms.
    # For native 8-bit samples the source tolerance 0.5/255 is exact RGB equality.
    if not isinstance(palette, dict) or len(palette) > 32:
        raise ValueError("Unsupported shader palette: expected at most 32 colors")
    colors = {}
    for source, target in palette.items():
        if not isinstance(source, str) or not isinstance(target, str):
            raise ValueError("Invalid upstream RGB palette value type")
        # Pinned color-utils.rgbHexToRgba maps malformed hex strings to black.
        # Preserve that behavior, including the shader's first matching entry.
        original = tuple(bytes.fromhex(source)) if re.fullmatch(r"[0-9a-fA-F]{6}", source) else (0, 0, 0)
        replacement = tuple(bytes.fromhex(target)) if re.fullmatch(r"[0-9a-fA-F]{6}", target) else (0, 0, 0)
        colors.setdefault(original, replacement)
    rgba = image.convert("RGBA")
    result = Image.new("RGBA", rgba.size)
    result.putdata([(*colors.get(pixel[:3], pixel[:3]), pixel[3]) if pixel[3] else pixel for pixel in rgba.getdata()])
    return result

def materialize(repo, key, facing, female, variant):
    if not re.fullmatch(r"[1-9][0-9]*(?:-[a-z0-9-]+)?", key) or facing not in ("front", "back") or variant not in (0, 1, 2):
        raise ValueError("Invalid appearance identity")
    lock = json.loads((ROOT / "project/data/assets/pokerogue-sprite-lock.json").read_text())
    revision = lock["revision"]
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip()
    if head != revision:
        raise ValueError("Asset repository differs from pinned revision")
    provenance = []
    game_repo = ROOT / "build/upstream/pokerogue"
    game_revision = "8555c08c823b856cbec4eb99ca84ea52a955836d"
    if subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=game_repo, text=True).strip() != game_revision:
        raise ValueError("Game source differs from inspected shader revision")
    for source in ("src/data/pokemon-species.ts", "src/utils/color-utils.ts", "src/pipelines/sprite.ts", "src/pipelines/glsl/sprite-frag-shader.frag"):
        raw = subprocess.check_output(["git", "show", game_revision + ":" + source], cwd=game_repo)
        provenance.append({"repository": "https://github.com/pagefaultgames/pokerogue", "revision": game_revision,
            "sourcePath": source, "sha256": hashlib.sha256(raw).hexdigest()})
    def read(source):
        raw = subprocess.check_output(["git", "show", revision + ":" + source], cwd=repo)
        provenance.append({"repository": lock["repository"], "revision": revision, "sourcePath": source,
            "sha256": hashlib.sha256(raw).hexdigest()})
        return raw
    master = json.loads(read("images/pokemon/variant/_masterlist.json"))
    config = master.get("back", {}) if facing == "back" else master
    if female:
        config = config.get("female", {})
    variants = config.get(key)
    if variants is not None and (not isinstance(variants, list) or len(variants) != 3 or any(type(v) is not int or v not in (0, 1, 2) for v in variants)):
        raise ValueError("Invalid upstream variant set")
    if variants is None and variant:
        raise ValueError("Missing upstream variant definition")
    prefix = ("back/" if facing == "back" else "") + ("female/" if female else "")
    mode = variants[variant] if variants else 0
    # PokemonSpecies.getSpriteId/getSpriteAtlasPath, non-experimental assets.
    source_root = ("back/" if facing == "back" else "") + ("shiny/" if (variants is None or (variant == 0 and mode == 0)) else "") + ("female/" if female else "") + key
    if mode == 2:
        source_root = "variant/" + prefix + key + "_" + str(variant + 1)
    manifest = read("images/pokemon/" + source_root + ".json")
    png = read("images/pokemon/" + source_root + ".png")
    image = Image.open(io.BytesIO(png)).convert("RGBA")
    if mode == 1:
        palettes = json.loads(read("images/pokemon/variant/" + prefix + key + ".json"))
        if str(variant) not in palettes:
            raise ValueError("Required upstream palette is missing")
        image = apply_palette(image, palettes[str(variant)])
    output = io.BytesIO()
    image.save(output, format="PNG")
    png_result = output.getvalue()
    return png_result, manifest, {"schemaVersion": 1, "atlasKey": key, "facing": facing,
        "female": female, "shiny": True, "variant": variant, "sourceMode": mode,
        "dimensions": list(image.size), "sources": provenance,
        "sourceSymbol": "PokemonSpecies.getSpriteId / rgbHexToRgba / SpritePipeline.onBatch / sprite-frag-shader.frag",
        "upstreamGameRevision": "8555c08c823b856cbec4eb99ca84ea52a955836d",
        "converterSHA256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "pngSHA256": hashlib.sha256(png_result).hexdigest(),
        "runtimeStatus": "SOURCE_MATERIALIZED_NOT_YET_T3X"}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("key")
    parser.add_argument("--facing", choices=("front", "back"), default="front")
    parser.add_argument("--female", action="store_true")
    parser.add_argument("--variant", type=int, choices=(0, 1, 2), required=True)
    args = parser.parse_args()
    png, manifest, provenance = materialize(ROOT / "build/upstream/pokerogue-assets", args.key, args.facing, args.female, args.variant)
    destination = ROOT / "build/upstream-assets/appearances" / args.facing / ("female" if args.female else "default")
    destination.mkdir(parents=True, exist_ok=True)
    name = args.key + "-shiny-v" + str(args.variant)
    (destination / (name + ".png")).write_bytes(png)
    (destination / (name + ".json")).write_bytes(manifest)
    (destination / (name + "-provenance.json")).write_text(json.dumps(provenance, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(destination / (name + ".png"))

if __name__ == "__main__":
    main()
