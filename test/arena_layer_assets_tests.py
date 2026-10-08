"""Validate actual static arena rasters and preserved animated atlas provenance."""
import hashlib
import json
import subprocess
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
report=json.loads((root / "docs/generated/ARENA_LAYER_REPORT.json").read_text())
static=animated=0
for row in report["files"]:
    source=root / "build/native-presentation/source" / row["sourcePath"]
    pinned=subprocess.check_output(["git","-C",str(root / "build/upstream/pokerogue-assets"),"show",report["revision"]+":"+row["sourcePath"]])
    assert source.read_bytes()==pinned
    assert hashlib.sha256(pinned).hexdigest()==row["sourceSHA256"]
    texture=root / "build/romfs" / row["runtimePath"].removeprefix("romfs:/")
    assert hashlib.sha256(texture.read_bytes()).hexdigest()==row["convertedSHA256"]
    if row["metadataPath"]:
        animated+=1
        assert row["runtimeScale"]==1.25 and row["resampling"]=="UNCHANGED_ATLAS"
        assert (root / "build/romfs" / row["metadataPath"].removeprefix("romfs:/")).is_file()
    else:
        static+=1
        assert row["runtimeScale"]==1 and row["resampling"]=="NEAREST"
        staged=root / "build/native-presentation/arena-layers" / source.name
        assert hashlib.sha256(staged.read_bytes()).hexdigest()==row["stagedSHA256"]
        with Image.open(source) as image,Image.open(staged) as raster:
            assert list(image.size)==row["sourceSize"]
            assert raster.size==(image.width*5//4,image.height*5//4)==(row["width"],row["height"])
            assert image.convert("RGBA").resize(raster.size,Image.Resampling.NEAREST).tobytes()==raster.convert("RGBA").tobytes()
print(f"PASS {static} static nearest layers, {animated} preserved animated atlases; native rendering pending")
