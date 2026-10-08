"""Physical compact icon comparison against the complete pinned icon catalog."""
import hashlib,io,json,subprocess,zipfile
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
report=json.loads((root / "docs/generated/COMPACT_ICON_REPORT.json").read_text())
source=report["sourceProvenance"]
archive=subprocess.check_output(["git","-C",str(root / "build/upstream/pokerogue-assets"),"archive","--format=zip",source["revision"],"images/pokemon/icons"])
pages=[]
for row in report["pages"]:
    original=root / "build/native-presentation" / f"icons-{row['page']}.png"
    staged=original.with_name(f"icons-compact-{row['page']}.png")
    assert hashlib.sha256(original.read_bytes()).hexdigest()==row["sourcePageSHA256"]
    assert hashlib.sha256(staged.read_bytes()).hexdigest()==row["stagedSHA256"]
    texture=root / "build/romfs" / row["runtimePath"].removeprefix("romfs:/")
    assert hashlib.sha256(texture.read_bytes()).hexdigest()==row["convertedSHA256"]
    pages.append(Image.open(staged).convert("RGBA"))
with zipfile.ZipFile(io.BytesIO(archive)) as upstream:
    for row in source["files"]:
        raw=upstream.read(row["sourcePath"])
        assert hashlib.sha256(raw).hexdigest()==row["sourceSHA256"]
        with Image.open(io.BytesIO(raw)) as original:
            expected=original.convert("RGBA").resize((row["width"]//2,row["height"]//2),Image.Resampling.NEAREST)
        x,y=row["x"]//2,row["y"]//2
        actual=pages[row["page"]].crop((x,y,x+expected.width,y+expected.height))
        assert actual.tobytes()==expected.tobytes(),(row["dex"],row["formIndex"])
print(f"PASS {len(source['files'])} compact icons match pinned nearest rasters; native rendering pending")
