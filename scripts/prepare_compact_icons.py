"""Deterministic 1:2 nearest icon pages for native 20x15 party presentation."""
import hashlib
import json
import subprocess
from pathlib import Path
from PIL import Image

def prepare(root):
    parent=root / "build/native-presentation/icon-provenance.json"
    provenance=json.loads(parent.read_text(encoding="utf-8"))
    for row in provenance["files"]:
        if any(row[key]%2 for key in ("x","y","width","height")):
            raise ValueError("Compact icon requires even source geometry")
    pages=[]
    for i in range(provenance["pages"]):
        original=root / "build/native-presentation" / f"icons-{i}.png"
        staged=original.with_name(f"icons-compact-{i}.png")
        with Image.open(original) as image:
            image.convert("RGBA").resize((image.width//2,image.height//2),Image.Resampling.NEAREST).save(staged)
        target=root / "build/romfs/presentation/icons" / f"icons-compact-{i}.t3x"
        subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(staged)],check=True,stdout=subprocess.DEVNULL)
        pages.append({"page":i,"sourcePageSHA256":hashlib.sha256(original.read_bytes()).hexdigest(),
            "stagedSHA256":hashlib.sha256(staged.read_bytes()).hexdigest(),
            "runtimePath":f"romfs:/presentation/icons/icons-compact-{i}.t3x",
            "convertedSHA256":hashlib.sha256(target.read_bytes()).hexdigest()})
    report={"schemaVersion":1,"sourceProvenance":provenance,"sourceProvenanceSHA256":hashlib.sha256(parent.read_bytes()).hexdigest(),
        "pages":pages,"adaptation":"NEAREST_1_2","runtimeValidation":"NOT_EXECUTED"}
    (root / "docs/generated/COMPACT_ICON_REPORT.json").write_bytes((json.dumps(report,sort_keys=True,indent=2)+"\n").encode())
    header="#pragma once\n// Generated compact raster pages; coordinates are original index / 2.\nnamespace Pokerogue3DS {\ninline constexpr const char* kCompactPokemonIconPages[]={\n"
    header+="\n".join(json.dumps(row["runtimePath"])+"," for row in pages)+"\n};\n}\n"
    (root / "project/generated/include/content/CompactPokemonIcons.hpp").write_bytes(header.encode())

if __name__=="__main__":prepare(Path(__file__).resolve().parents[1])
