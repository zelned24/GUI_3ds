"""Verify physical item provenance and deterministic indexing; no ARM build."""
import hashlib
import importlib.util
import io
import json
import subprocess
import zipfile
from pathlib import Path
from PIL import Image
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("item_index",root/"scripts/prepare_item_icon_index.py")
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
report_path=root/"docs/generated/ITEM_ICON_TEXTURE_REPORT.json"
header_path=root/"project/generated/include/content/ItemIconTextures.hpp"
report=json.loads(report_path.read_text())
assert report["revision"]==module.REVISION
archive=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"archive","--format=zip",report["revision"],"images/items"])
with zipfile.ZipFile(io.BytesIO(archive)) as sources:
    paths=sorted(name for name in sources.namelist() if name.endswith(".png"))
    assert sorted(row["sourcePath"] for row in report["files"])==paths
    keys=[row["key"] for row in report["files"]]
    assert keys==sorted(set(keys))
    for row in report["files"]:
        source=sources.read(row["sourcePath"])
        assert hashlib.sha256(source).hexdigest()==row["sourceSHA256"]
        with Image.open(io.BytesIO(source)) as image:
            assert image.size==(row["width"],row["height"])
        physical=root/"build"/row["runtimePath"].replace("romfs:/","romfs/")
        assert hashlib.sha256(physical.read_bytes()).hexdigest()==row["convertedSHA256"]
        assert row["runtimePath"] in header_path.read_text()
before=(report_path.read_bytes(),header_path.read_bytes())
for _ in range(2):
    module.prepare(root)
    assert before==(report_path.read_bytes(),header_path.read_bytes())
print(f"PASS: {len(keys)} physical item sources, hashes, paths and deterministic generations")
