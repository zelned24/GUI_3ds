"""Physical asset verification only; no C++ build or emulator execution."""
import hashlib
import io
import json
import subprocess
import re
import zipfile
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
report=json.loads((ROOT / "docs/generated/APPEARANCE_ICON_CONVERSION_REPORT.json").read_text(encoding="utf-8"))
archive=subprocess.check_output(["git","-C",str(ROOT / "build/upstream/pokerogue-assets"),"archive","--format=zip",report["revision"],"images/pokemon/icons"])
images={}
for page in report["pages"]:
    png=ROOT / "build/native-presentation/appearance-icons" / f"appearance-icons-{page['page']}.png"
    texture=ROOT / "build/romfs" / page["runtimePath"].removeprefix("romfs:/")
    assert hashlib.sha256(png.read_bytes()).hexdigest()==page["stagedSHA256"]
    assert hashlib.sha256(texture.read_bytes()).hexdigest()==page["convertedSHA256"]
    with Image.open(png) as image: images[page["page"]]=image.convert("RGBA")
    assert images[page["page"]].size==(512,512)
with zipfile.ZipFile(io.BytesIO(archive)) as files:
    assert sorted(row["sourcePath"] for row in report["files"])==sorted(name for name in files.namelist() if name.endswith(".png"))
    for row in report["files"]:
        raw=files.read(row["sourcePath"])
        assert hashlib.sha256(raw).hexdigest()==row["sourceSHA256"]
        with Image.open(io.BytesIO(raw)) as original:
            expected=original.convert("RGBA")
        actual=images[row["page"]].crop((row["x"],row["y"],row["x"]+row["width"],row["y"]+row["height"]))
        assert actual.size==expected.size and actual.tobytes()==expected.tobytes(),row["sourcePath"]
print("PASS exact pinned pixels and conversion hashes:",len(report["files"]),"icons")

header=(ROOT / "project/generated/include/content/AppearanceIcons.hpp").read_text(encoding="utf-8")
rows=re.findall(r'    \{"([^"\n]+)",(\d+),(\d+),(\d+),(\d+),(\d+)\},',header)
expected=[(row["sourcePath"].removeprefix("images/pokemon/icons/"),*(str(row[field]) for field in ("page","x","y","width","height"))) for row in report["files"]]
assert rows==expected, "Generated C++ index differs from conversion report"
for page in report["pages"]: assert json.dumps(page["runtimePath"])+"," in header
for row in report["files"]:
    assert row["page"]<len(report["pages"])
    assert row["x"]+row["width"]<=512 and row["y"]+row["height"]<=512
print("PASS generated appearance icon index and physical page bounds")
