"""Validate actual static arena rasters and preserved animated atlas provenance."""
import hashlib
import json
import subprocess
import struct
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
    tw=max(8,1 << (row["width"]-1).bit_length())
    th=max(8,1 << (row["height"]-1).bit_length())
    assert row["estimatedTextureSize"]==[tw,th]
    assert row["estimatedTextureBytes"]==tw*th*4
    if row["metadataPath"]:
        animated+=1
        assert row["runtimeScale"]==1 and row["resampling"]=="NEAREST_FRAMES"
        metadata=root / "build/romfs" / row["metadataPath"].removeprefix("romfs:/")
        assert hashlib.sha256(metadata.read_bytes()).hexdigest()==row["metadataSHA256"]
        raw=metadata.read_bytes()
        assert struct.unpack_from("<8sIHHI",raw)==(b"P3ATLAS1",1,row["width"],row["height"],len(row["runtimeFrames"]))
        assert len(raw)==84+32*len(row["runtimeFrames"])
        for i,frame in enumerate(row["runtimeFrames"]):
            b=frame["frame"];src=frame["sourceSize"];trim=frame["spriteSourceSize"]
            assert struct.unpack_from("<12s10H",raw,84+32*i)==(frame["filename"].encode().ljust(12,b"\0"),b["x"],b["y"],b["w"],b["h"],src["w"],src["h"],trim["x"],trim["y"],0,0)
        manifest=source.with_suffix(".json")
        pinned_manifest=subprocess.check_output(["git","-C",str(root / "build/upstream/pokerogue-assets"),"show",report["revision"]+":"+row["sourcePath"].removesuffix(".png")+".json"])
        assert manifest.read_bytes()==pinned_manifest
        cell_w=max(f["frame"]["w"] for f in row["runtimeFrames"])
        cell_h=max(f["frame"]["h"] for f in row["runtimeFrames"])
        count=len(row["runtimeFrames"])
        estimates=[]
        for columns in range(1,count+1):
            w,h=columns*cell_w,((count+columns-1)//columns)*cell_h
            if w<=1024 and h<=1024:
                estimates.append(max(8,1 << (w-1).bit_length())*max(8,1 << (h-1).bit_length())*4)
        assert row["estimatedTextureBytes"]==min(estimates)
        original_frames=sorted(json.loads(pinned_manifest)["textures"][0]["frames"],key=lambda frame:frame["filename"])
        assert original_frames==row["sourceFrames"]
        staged=root / "build/native-presentation/arena-layers" / source.name
        assert hashlib.sha256(staged.read_bytes()).hexdigest()==row["stagedSHA256"]
        with Image.open(source) as atlas,Image.open(staged) as packed:
            for original,adapted in zip(original_frames,row["runtimeFrames"]):
                assert original["filename"]==adapted["filename"]
                b=original["frame"];src=original["sourceSize"];trim=original["spriteSourceSize"]
                expected=Image.new("RGBA",(src["w"],src["h"]))
                expected.paste(atlas.crop((b["x"],b["y"],b["x"]+b["w"],b["y"]+b["h"])),(trim["x"],trim["y"]))
                expected=expected.resize((src["w"]*5//4,src["h"]*5//4),Image.Resampling.NEAREST)
                b=adapted["frame"];trim=adapted["spriteSourceSize"];src=adapted["sourceSize"]
                actual=Image.new("RGBA",(src["w"],src["h"]))
                actual.paste(packed.crop((b["x"],b["y"],b["x"]+b["w"],b["y"]+b["h"])),(trim["x"],trim["y"]))
                assert actual.tobytes()==expected.tobytes()
    else:
        static+=1
        assert row["runtimeScale"]==1 and row["resampling"]=="NEAREST"
        staged=root / "build/native-presentation/arena-layers" / source.name
        assert hashlib.sha256(staged.read_bytes()).hexdigest()==row["stagedSHA256"]
        with Image.open(source) as image,Image.open(staged) as raster:
            assert list(image.size)==row["sourceSize"]
            assert raster.size==(image.width*5//4,image.height*5//4)==(row["width"],row["height"])
            assert image.convert("RGBA").resize(raster.size,Image.Resampling.NEAREST).tobytes()==raster.convert("RGBA").tobytes()
print(f"PASS {static} static nearest layers, {animated} adapted animated atlases (all frames); native rendering pending")
