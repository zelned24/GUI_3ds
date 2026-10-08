"""Pinned arena layer conversion; static bases adapted offline, animated atlases preserved."""
import hashlib
import json
import struct
import subprocess
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
REPOSITORY="https://github.com/pagefaultgames/pokerogue-assets"
REVISION="056a1f408f26a3be4fef243f7462cb43608c7928"
output=ROOT / "build/native-presentation/source"
layer_rows=[]
unsupported=[]
for png in sorted((output / "images/arenas").glob("*.png")):
    if not png.stem.endswith(("_a","_b")): continue
    width,height=struct.unpack(">II",png.read_bytes()[16:24])
    if width>1024 or height>1024:
        unsupported.append({"sourcePath":png.relative_to(output).as_posix(),"reason":"Texture requires paged conversion"})
        continue
    source_size=(width,height)
    manifest=png.with_suffix(".json")
    raster_source=png
    if not manifest.exists():
        staged=ROOT / "build/native-presentation/arena-layers" / png.name
        staged.parent.mkdir(parents=True,exist_ok=True)
        width,height=width*5//4,height*5//4
        with Image.open(png) as image:
            image.convert("RGBA").resize((width,height),Image.Resampling.NEAREST).save(staged)
        raster_source=staged
    target=ROOT / "build/romfs/presentation/arenas" / (png.stem+".t3x")
    subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(raster_source)],check=True,stdout=subprocess.DEVNULL)
    layer_rows.append({"key":png.stem,"sourcePath":png.relative_to(output).as_posix(),"sourceSHA256":hashlib.sha256(png.read_bytes()).hexdigest(),"width":width,"height":height,"runtimePath":"romfs:/presentation/arenas/"+png.stem+".t3x","convertedSHA256":hashlib.sha256(target.read_bytes()).hexdigest()})
    row=layer_rows[-1]
    row["metadataPath"]=None
    row["sourceSize"]=list(source_size)
    row["runtimeScale"]=1.25 if manifest.exists() else 1
    row["resampling"]="UNCHANGED_ATLAS" if manifest.exists() else "NEAREST"
    row["stagedSHA256"]=hashlib.sha256(raster_source.read_bytes()).hexdigest()
    manifest=png.with_suffix(".json")
    if manifest.exists():
        atlas=json.loads(manifest.read_text(encoding="utf-8"))["textures"][0]
        frames=sorted(atlas["frames"],key=lambda frame:frame["filename"])
        binary=bytearray(struct.pack("<8sIHHI",b"P3ATLAS1",1,width,height,len(frames)))
        binary.extend(hashlib.sha256(png.read_bytes()).digest())
        binary.extend(hashlib.sha256(manifest.read_bytes()).digest())
        for frame in frames:
            if frame.get("rotated"): raise ValueError("Rotated arena frame requires explicit adapter")
            bounds=frame["frame"]; source=frame["sourceSize"]; trim=frame["spriteSourceSize"]
            filename=frame["filename"].encode("ascii")
            if len(filename)>=12: raise ValueError("Arena frame name exceeds metadata contract")
            binary.extend(struct.pack("<12s10H",filename,bounds["x"],bounds["y"],bounds["w"],bounds["h"],source["w"],source["h"],trim["x"],trim["y"],0,0))
        metadata=target.with_suffix(".p3a")
        metadata.write_bytes(binary)
        row["metadataPath"]="romfs:/presentation/arenas/"+metadata.name
        row["manifestSHA256"]=hashlib.sha256(manifest.read_bytes()).hexdigest()
        row["frames"]=len(frames)
        row["frameRate"]=12 # ArenaBase.setBiome pinned source, not Pokémon's 10 FPS.

header="// Generated pinned static arena layers.\n#pragma once\n#include \"content/ArenaTextures.hpp\"\nnamespace Pokerogue3DS {\nstruct ArenaLayerTextureDefinition { const char* key; const char* path; uint16_t width,height; const char* metadataPath; };\ninline constexpr ArenaLayerTextureDefinition kArenaLayerTextures[] = {\n"
header+="\n".join('    {"%s", "%s", %d, %d, %s},' % (r["key"],r["runtimePath"],r["width"],r["height"],json.dumps(r["metadataPath"]) if r["metadataPath"] else "nullptr") for r in layer_rows)
header+="\n};\n}\n"
(ROOT / "project/generated/include/content/ArenaLayerTextures.hpp").write_text(header,encoding="utf-8",newline="\n")
(output.parent / "layer-provenance.json").write_text(json.dumps({"repository":REPOSITORY,"revision":REVISION,"files":layer_rows,"unsupported":unsupported},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
(ROOT / "docs/generated/ARENA_LAYER_REPORT.json").write_bytes((json.dumps({"repository":REPOSITORY,"revision":REVISION,"files":layer_rows,"unsupported":unsupported,"runtimeValidation":"NOT_EXECUTED"},sort_keys=True,indent=2)+"\n").encode())
print(f"Converted {len(layer_rows)} arena layers ({sum(bool(row["metadataPath"]) for row in layer_rows)} animated); {len(unsupported)} unsupported")

