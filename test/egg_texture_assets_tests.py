"""Verify native egg pixels reconstructed from pinned trimmed atlas frames."""
import hashlib,importlib.util,io,json,subprocess
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
report_path=root/"docs/generated/EGG_TEXTURE_REPORT.json"
report=json.loads(report_path.read_text(encoding="utf-8"))
assert len(report["files"])==10
for row in report["files"]:
    def source(path):
        return subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"show",report["revision"]+":"+path])
    png=source(row["sourcePath"]);manifest=source(row["manifestPath"])
    assert hashlib.sha256(png).hexdigest()==row["sourceSHA256"]
    assert hashlib.sha256(manifest).hexdigest()==row["manifestSHA256"]
    frames=json.loads(manifest)["textures"][0]["frames"]
    frame=next(f for f in frames if f["filename"]==row["key"])
    assert frame==row["frame"] and not frame["rotated"]
    b=frame["frame"];trim=frame["spriteSourceSize"]
    expected=Image.new("RGBA",(row["width"],row["height"]))
    with Image.open(io.BytesIO(png)) as atlas:
        expected.paste(atlas.convert("RGBA").crop((b["x"],b["y"],b["x"]+b["w"],b["y"]+b["h"])),(trim["x"],trim["y"]))
    staged=root/"build/native-presentation/egg-frames"/(row["atlas"]+"-"+row["key"]+".png")
    with Image.open(staged) as actual: assert actual.convert("RGBA").tobytes()==expected.tobytes()
    assert hashlib.sha256(staged.read_bytes()).hexdigest()==row["stagedSHA256"]
    physical=root/"build"/row["runtimePath"].replace("romfs:/","romfs/")
    assert hashlib.sha256(physical.read_bytes()).hexdigest()==row["convertedSHA256"]
header=root/"project/generated/include/content/EggTextures.hpp"
before=(header.read_bytes(),report_path.read_bytes())
spec=importlib.util.spec_from_file_location("egg_textures",root/"scripts/prepare_egg_textures.py")
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
module.prepare(root)
assert before==(header.read_bytes(),report_path.read_bytes())
print("PASS: 10 original egg frames, native pixels, trim, provenance and deterministic conversion; GPU pending")
