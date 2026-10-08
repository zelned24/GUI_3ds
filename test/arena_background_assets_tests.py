"""Physical raster/provenance checks; does not execute the 3DS renderer."""
import hashlib
import json
import subprocess
from pathlib import Path
from PIL import Image, ImageChops
root = Path(__file__).resolve().parents[1]
report = json.loads((root / "docs/generated/ARENA_BACKGROUND_REPORT.json").read_text())
assert report["files"]
for row in report["files"]:
    source = root / "build/native-presentation/source" / row["sourcePath"]
    pinned = subprocess.check_output(["git", "-C", str(root / "build/upstream/pokerogue-assets"), "show", report["revision"] + ":" + row["sourcePath"]])
    assert source.read_bytes() == pinned
    assert hashlib.sha256(pinned).hexdigest() == row["sourceSHA256"]
    staged = root / "build/native-presentation/arena-backgrounds" / (row["key"] + ".png")
    assert hashlib.sha256(staged.read_bytes()).hexdigest() == row["stagedSHA256"]
    with Image.open(source) as original, Image.open(staged) as raster:
        assert list(original.size) == row["sourceSize"]
        assert raster.size == (row["width"], row["height"])
        expected = original.convert("RGBA").resize(raster.size, Image.Resampling.NEAREST)
        assert expected.tobytes() == raster.convert("RGBA").tobytes()
    title_png = staged.with_name(row["key"] + "-title.png")
    assert hashlib.sha256(title_png.read_bytes()).hexdigest() == row["titleStagedSHA256"]
    with Image.open(source) as original, Image.open(title_png) as raster:
        assert raster.size == (400, 240)
        crop_width = original.height * 400 / 240
        left = (original.width - crop_width) / 2
        assert row["titleSourceBox"] == [left, 0, left + crop_width, original.height]
        expected = original.convert("RGBA").resize((400, 240), Image.Resampling.NEAREST, box=tuple(row["titleSourceBox"]))
        assert expected.tobytes() == raster.convert("RGBA").tobytes()
    title_texture = root / "build/romfs" / row["titleRuntimePath"].removeprefix("romfs:/")
    assert hashlib.sha256(title_texture.read_bytes()).hexdigest() == row["titleConvertedSHA256"]
    texture = root / "build/romfs" / row["runtimePath"].removeprefix("romfs:/")
    assert hashlib.sha256(texture.read_bytes()).hexdigest() == row["convertedSHA256"]
print(f"PASS {len(report['files'])} pinned nearest arena rasters and texture hashes; native rendering pending")
