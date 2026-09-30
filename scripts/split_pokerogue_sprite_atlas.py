"""Repack one pinned oversized PokéRogue animation atlas into 3DS pages.

The original PNG and manifest remain untouched. P3ATLAS2 retains their hashes
and stores a page number in bits 1..4 of each existing frame flags field.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

from PIL import Image


ROOT = Path(__file__).resolve().parent.parent
EDGE = 1024
HEADER = 84
RECORD = 32


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("atlas_key", help="Pinned numeric or named-form atlas key")
    parser.add_argument("facing", choices=("front", "back"))
    parser.add_argument("--tex3ds", default="tex3ds")
    args = parser.parse_args()
    if not args.atlas_key or any(c not in "0123456789abcdefghijklmnopqrstuvwxyz-" for c in args.atlas_key):
        parser.error("unsafe atlas key")

    staged = json.loads((ROOT / "build/upstream-assets/staged-sprite-assets.json").read_text())
    matches = [entry for entry in staged["assets"]
               if entry["atlasKey"] == args.atlas_key and entry["facing"] == args.facing]
    if len(matches) != 1:
        raise ValueError("atlas is missing or ambiguous in the pinned staging inventory")
    asset = matches[0]
    source_path = ROOT / asset["sourcePath"]
    source_bytes = source_path.read_bytes()
    if sha256(source_bytes) != asset["expectedSha256"].removeprefix("sha256:"):
        raise ValueError("pinned source PNG hash differs from staged inventory")
    original_metadata = (ROOT / asset["metadataPath"]).read_bytes()
    if sha256(original_metadata) != asset["metadataSha256"]:
        raise ValueError("pinned atlas metadata hash differs from staged inventory")
    if original_metadata[:8] != b"P3ATLAS1" or struct.unpack_from("<I", original_metadata, 8)[0] != 1:
        raise ValueError("expected the staged P3ATLAS1 format")
    source_width, source_height = struct.unpack_from("<HH", original_metadata, 12)
    count = struct.unpack_from("<I", original_metadata, 16)[0]
    if source_width != asset["width"] or source_height != asset["height"] or count != asset["frameCount"]:
        raise ValueError("staged atlas dimensions/count differ")
    if original_metadata[20:52].hex() != sha256(source_bytes) or len(original_metadata) != HEADER + RECORD * count:
        raise ValueError("staged atlas source hash or file length differs")
    if source_width <= EDGE and source_height <= EDGE:
        raise ValueError("this atlas fits on one texture; splitting is unnecessary")

    with Image.open(source_path) as decoded:
        image = decoded.convert("RGBA")
    if image.size != (source_width, source_height):
        raise ValueError("decoded PNG dimensions differ")

    pages = [Image.new("RGBA", (EDGE, EDGE), (0, 0, 0, 0))]
    metadata = bytearray(original_metadata)
    metadata[:8] = b"P3ATLAS2"
    struct.pack_into("<IHH", metadata, 8, 2, EDGE, EDGE)
    x = y = row_height = 0
    placements = []
    # Place frames in playback-name order so changing pages is rare during the
    # pinned 10 FPS loop, even when TexturePacker stored them in another order.
    ordered = sorted(range(count), key=lambda item:
                     original_metadata[HEADER + item * RECORD:HEADER + item * RECORD + 12])
    for index in ordered:
        offset = HEADER + index * RECORD
        frame_x, frame_y, width, height = struct.unpack_from("<HHHH", original_metadata, offset + 12)
        flags = struct.unpack_from("<H", original_metadata, offset + 30)[0]
        if flags & ~1 or not width or not height or frame_x + width > source_width or frame_y + height > source_height:
            raise ValueError(f"invalid staged frame {index}")
        reserved_width, reserved_height = width + 1, height + 1
        if reserved_width > EDGE or reserved_height > EDGE:
            raise ValueError(f"frame {index} itself exceeds the 3DS page")
        if x + reserved_width > EDGE:
            x, y, row_height = 0, y + row_height, 0
        if y + reserved_height > EDGE:
            pages.append(Image.new("RGBA", (EDGE, EDGE), (0, 0, 0, 0)))
            x = y = row_height = 0
        page_index = len(pages) - 1
        if page_index > 15:
            raise ValueError("P3ATLAS2 supports at most 16 pages")
        pages[-1].paste(image.crop((frame_x, frame_y, frame_x + width, frame_y + height)), (x, y))
        placements.append((index, page_index, frame_x, frame_y, width, height, x, y))
        struct.pack_into("<HH", metadata, offset + 12, x, y)
        struct.pack_into("<H", metadata, offset + 30, (page_index << 1) | flags)
        x += reserved_width
        row_height = max(row_height, reserved_height)

    for index, page_index, source_x, source_y, width, height, target_x, target_y in placements:
        original_pixels = image.crop((source_x, source_y, source_x + width, source_y + height)).tobytes()
        copied_pixels = pages[page_index].crop((target_x, target_y, target_x + width, target_y + height)).tobytes()
        if original_pixels != copied_pixels:
            raise ValueError(f"split page changed pixels for frame {index}")

    source_dir = ROOT / "build/upstream-assets/split-pages" / args.facing
    texture_dir = ROOT / "build/romfs/sprites/pokemon"
    if args.facing == "back":
        texture_dir /= "back"
    source_dir.mkdir(parents=True, exist_ok=True)
    texture_dir.mkdir(parents=True, exist_ok=True)
    for directory, suffix in ((source_dir, ".png"), (texture_dir, ".t3x")):
        for obsolete in directory.glob(f"{args.atlas_key}-p*{suffix}"):
            page_text = obsolete.name.removeprefix(f"{args.atlas_key}-p").removesuffix(suffix)
            if page_text.isdecimal() and int(page_text) >= len(pages):
                obsolete.unlink()
    report = {"schemaVersion": 2, "repository": staged["repository"], "revision": staged["revision"],
              "atlasKey": args.atlas_key, "facing": args.facing,
              "sourcePath": asset["upstreamImagePath"], "sourceSha256": sha256(source_bytes),
              "manifestPath": asset["manifestPath"], "manifestSha256": asset["manifestSha256"],
              "frameCount": count, "pageEdge": EDGE, "pages": []}
    for index, page in enumerate(pages):
        page_png = source_dir / f"{args.atlas_key}-p{index}.png"
        page_t3x = texture_dir / f"{args.atlas_key}-p{index}.t3x"
        page.save(page_png, format="PNG", optimize=False)
        subprocess.run([args.tex3ds, "--atlas", "-f", "rgba4", "-z", "auto",
                        "-o", str(page_t3x), str(page_png)], check=True, capture_output=True)
        report["pages"].append({"index": index,
                                "pngPath": str(page_png.relative_to(ROOT)).replace("\\", "/"),
                                "pngSha256": sha256(page_png.read_bytes()),
                                "texturePath": str(page_t3x.relative_to(ROOT)).replace("\\", "/"),
                                "textureSha256": sha256(page_t3x.read_bytes()),
                                "textureBytes": page_t3x.stat().st_size})
    metadata_path = ROOT / "build" / asset["romfsMetadataPath"]
    metadata_path.parent.mkdir(parents=True, exist_ok=True)
    metadata_path.write_bytes(metadata)
    report["metadataPath"] = str(metadata_path.relative_to(ROOT)).replace("\\", "/")
    report["metadataSha256"] = sha256(metadata)
    report_path = ROOT / "build/upstream-assets" / f"split-{args.atlas_key}-{args.facing}.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    print(f"{args.atlas_key}:{args.facing}: {count} pinned frames across {len(pages)} 3DS pages; {report_path}")


if __name__ == "__main__":
    main()
