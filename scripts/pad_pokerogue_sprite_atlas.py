"""Recover a pinned atlas whose PNG is one pixel smaller than its manifest.

The upstream PNG remains unchanged. Only transparent pixels are added to the
right/bottom of a derived conversion source; both hashes remain in the report.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

from PIL import Image


ROOT = Path(__file__).resolve().parent.parent
SIGNATURE = bytes.fromhex("89504e470d0a1a0a")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("atlas_key")
    parser.add_argument("facing", choices=("front", "back"))
    args = parser.parse_args()
    if not re.fullmatch(r"[1-9][0-9]*(?:-[a-z0-9-]+)?", args.atlas_key):
        parser.error("unsafe atlas key")
    report_path = ROOT / "build/upstream-assets/staged-sprite-assets.json"
    report = json.loads(report_path.read_text())
    candidates = [entry for entry in report["unsupported"]
                  if entry["key"] == args.atlas_key and entry["facing"] == args.facing
                  and entry["classification"] == "INVALID_UPSTREAM_FRAME_BOUNDS"]
    if len(candidates) != 1:
        raise ValueError("expected exactly one explicit frame-bounds exclusion")
    excluded = candidates[0]
    source_path = ROOT / "build/upstream/pokerogue-assets" / excluded["imagePath"]
    manifest_path = ROOT / "build/upstream/pokerogue-assets" / excluded["manifestPath"]
    source_bytes, manifest_bytes = source_path.read_bytes(), manifest_path.read_bytes()
    if sha256(source_bytes) != excluded["imageSha256"] or sha256(manifest_bytes) != excluded["manifestSha256"]:
        raise ValueError("pinned source hashes changed")
    if source_bytes[:8] != SIGNATURE:
        raise ValueError("invalid pinned PNG")
    physical_width, physical_height = struct.unpack_from(">II", source_bytes, 16)
    manifest = json.loads(manifest_bytes)
    if len(manifest.get("textures", [])) != 1:
        raise ValueError("unsupported atlas layout")
    texture = manifest["textures"][0]
    target_width, target_height = texture["size"]["w"], texture["size"]["h"]
    if not (physical_width <= target_width <= physical_width + 1
            and physical_height <= target_height <= physical_height + 1
            and (target_width, target_height) != (physical_width, physical_height)):
        raise ValueError("manifest is not a one-pixel transparent-padding case")
    if texture["image"] != f"{args.atlas_key}.png" or target_width > 1024 or target_height > 1024:
        raise ValueError("unexpected image reference or 3DS texture size")
    frames = texture["frames"]
    if not frames or len(frames) > 1024:
        raise ValueError("unsupported frame count")
    with Image.open(source_path) as decoded:
        source_image = decoded.convert("RGBA")
    if source_image.size != (physical_width, physical_height):
        raise ValueError("decoded PNG dimensions changed")
    derived = Image.new("RGBA", (target_width, target_height), (0, 0, 0, 0))
    derived.paste(source_image, (0, 0))
    padded_path = ROOT / "build/upstream-assets/padded-sources" / args.facing / f"{args.atlas_key}.png"
    padded_path.parent.mkdir(parents=True, exist_ok=True)
    derived.save(padded_path, format="PNG", optimize=False)
    padded_bytes = padded_path.read_bytes()
    metadata = bytearray(84 + len(frames) * 32)
    metadata[:8] = b"P3ATLAS1"
    struct.pack_into("<IHHI", metadata, 8, 1, target_width, target_height, len(frames))
    metadata[20:52] = bytes.fromhex(sha256(padded_bytes))
    metadata[52:84] = bytes.fromhex(sha256(manifest_bytes))
    names = set()
    for index, frame in enumerate(frames):
        name = frame["filename"]
        rect, trim, canvas = frame["frame"], frame["spriteSourceSize"], frame["sourceSize"]
        if not re.fullmatch(r"[A-Za-z0-9_.-]{1,11}", name) or name in names or frame["rotated"]:
            raise ValueError(f"unsupported frame identity {index}")
        names.add(name)
        values = (rect["x"], rect["y"], rect["w"], rect["h"],
                  canvas["w"], canvas["h"], trim["x"], trim["y"], frame.get("duration", 0))
        if (not all(isinstance(value, int) and 0 <= value <= 65535 for value in values)
                or rect["w"] < 1 or rect["h"] < 1 or canvas["w"] < 1 or canvas["h"] < 1
                or rect["x"] + rect["w"] > target_width or rect["y"] + rect["h"] > target_height
                or trim["x"] + rect["w"] > canvas["w"] + 1
                or trim["y"] + rect["h"] > canvas["h"] + 1
                or (trim["w"], trim["h"]) != (rect["w"], rect["h"])):
            raise ValueError(f"invalid pinned frame geometry {index}")
        offset = 84 + index * 32
        metadata[offset:offset + len(name)] = name.encode("ascii")
        struct.pack_into("<9H", metadata, offset + 12, *values)
        struct.pack_into("<H", metadata, offset + 30, int(frame["trimmed"]))
    metadata_relative = f"build/upstream-assets/atlas-metadata/{args.facing}/{args.atlas_key}.p3a"
    romfs_metadata = f"romfs/sprites/pokemon/atlas/{args.facing}/{args.atlas_key}.p3a"
    for relative in (metadata_relative, f"build/{romfs_metadata}"):
        destination = ROOT / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(metadata)
    entry = {"assetId": f"pokemon_sprite_{args.atlas_key}_{args.facing}",
             "speciesId": int(args.atlas_key.split("-")[0]), "atlasKey": args.atlas_key,
             "facing": args.facing, "repository": report["repository"], "revision": report["revision"],
             "manifestPath": excluded["manifestPath"], "manifestSha256": sha256(manifest_bytes),
             "sourcePath": str(padded_path.relative_to(ROOT)).replace("\\", "/"),
             "upstreamImagePath": excluded["imagePath"],
             "upstreamImageSha256": sha256(source_bytes),
             "expectedSha256": f"sha256:{sha256(padded_bytes)}",
             "sourceAdjustment": "TRANSPARENT_PAD_TO_PINNED_MANIFEST_SIZE",
             "width": target_width, "height": target_height,
             "atlasFormat": "TEXTUREPACKER_TEXTURES", "referencedImage": texture["image"],
             "imageReferenceOverridden": False, "metadataPath": metadata_relative,
             "metadataSha256": sha256(metadata), "romfsMetadataPath": romfs_metadata,
             "romfsPath": f"romfs/sprites/pokemon/{'back/' if args.facing == 'back' else ''}{args.atlas_key}.t3x",
             "frameCount": len(frames)}
    if any(asset["atlasKey"] == args.atlas_key and asset["facing"] == args.facing for asset in report["assets"]):
        raise ValueError("atlas was already staged")
    report["assets"].append(entry)
    report["assets"].sort(key=lambda asset: (int(asset["atlasKey"].split("-")[0]), asset["atlasKey"], asset["facing"]))
    report["staged"] = len(report["assets"])
    report["unsupported"] = [item for item in report["unsupported"] if item is not excluded]
    report["warnings"].append({"key": args.atlas_key, "facing": args.facing,
                               "classification": "PINNED_PNG_ONE_PIXEL_SMALLER_THAN_MANIFEST",
                               "physicalWidth": physical_width, "physicalHeight": physical_height,
                               "declaredWidth": target_width, "declaredHeight": target_height,
                               "upstreamImageSha256": sha256(source_bytes),
                               "derivedImageSha256": sha256(padded_bytes)})
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Recovered {args.atlas_key}:{args.facing} with transparent padding; {len(frames)} frames; {report_path}")


if __name__ == "__main__":
    main()
