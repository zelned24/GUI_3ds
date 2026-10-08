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
def adapt_frames(png, manifest, destination):
    originals=sorted(json.loads(manifest.read_text(encoding="utf-8"))["textures"][0]["frames"],key=lambda frame:frame["filename"])
    crops=[]; adapted=[]
    with Image.open(png) as atlas:
        for frame in originals:
            if frame.get("rotated"): raise ValueError("Rotated arena frame")
            b=frame["frame"]; src=frame["sourceSize"]; trim=frame["spriteSourceSize"]
            full=Image.new("RGBA",(src["w"],src["h"]))
            full.paste(atlas.crop((b["x"],b["y"],b["x"]+b["w"],b["y"]+b["h"])),(trim["x"],trim["y"]))
            full=full.resize((src["w"]*5//4,src["h"]*5//4),Image.Resampling.NEAREST)
            box=full.getchannel("A").getbbox()
            if box is None: raise ValueError("Empty arena animation frame")
            crop=full.crop(box);crops.append(crop)
            adapted.append({"filename":frame["filename"],"sourceSize":{"w":full.width,"h":full.height},
                "spriteSourceSize":{"x":box[0],"y":box[1],"w":crop.width,"h":crop.height},
                "rotated":False,"trimmed":True})
    cell_w=max(c.width for c in crops);cell_h=max(c.height for c in crops)
    def power_of_two(value): return max(8,1 << (value-1).bit_length())
    candidates=[]
    for columns in range(1,len(crops)+1):
        rows=(len(crops)+columns-1)//columns
        w,h=columns*cell_w,rows*cell_h
        if w<=1024 and h<=1024:
            tw,th=power_of_two(w),power_of_two(h)
            candidates.append((tw*th,max(tw,th),w*h,columns,rows))
    if not candidates: raise ValueError("Adapted atlas requires paging")
    _,_,_,columns,rows=min(candidates)
    packed=Image.new("RGBA",(columns*cell_w,rows*cell_h))
    if packed.width>1024 or packed.height>1024: raise ValueError("Adapted atlas requires paging")
    for i,(crop,frame) in enumerate(zip(crops,adapted)):
        x=i%columns*cell_w;y=i//columns*cell_h
        packed.paste(crop,(x,y));frame["frame"]={"x":x,"y":y,"w":crop.width,"h":crop.height}
    destination.parent.mkdir(parents=True,exist_ok=True);packed.save(destination)
    return adapted,packed.size,originals

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
    adapted_frames=None
    if not manifest.exists():
        staged=ROOT / "build/native-presentation/arena-layers" / png.name
        staged.parent.mkdir(parents=True,exist_ok=True)
        width,height=width*5//4,height*5//4
        with Image.open(png) as image:
            image.convert("RGBA").resize((width,height),Image.Resampling.NEAREST).save(staged)
        raster_source=staged
    else:
        staged=ROOT / "build/native-presentation/arena-layers" / png.name
        adapted_frames,(width,height),original_frames=adapt_frames(png,manifest,staged)
        raster_source=staged
    target=ROOT / "build/romfs/presentation/arenas" / (png.stem+".t3x")
    subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(raster_source)],check=True,stdout=subprocess.DEVNULL)
    layer_rows.append({"key":png.stem,"sourcePath":png.relative_to(output).as_posix(),"sourceSHA256":hashlib.sha256(png.read_bytes()).hexdigest(),"width":width,"height":height,"runtimePath":"romfs:/presentation/arenas/"+png.stem+".t3x","convertedSHA256":hashlib.sha256(target.read_bytes()).hexdigest()})
    row=layer_rows[-1]
    row["metadataPath"]=None
    row["sourceSize"]=list(source_size)
    row["runtimeScale"]=1
    row["resampling"]="NEAREST_FRAMES" if manifest.exists() else "NEAREST"
    texture_w=max(8,1 << (width-1).bit_length())
    texture_h=max(8,1 << (height-1).bit_length())
    row["estimatedTextureSize"]=[texture_w,texture_h]
    row["estimatedTextureBytes"]=texture_w*texture_h*4
    row["textureFormat"]="RGBA8"
    row["stagedSHA256"]=hashlib.sha256(raster_source.read_bytes()).hexdigest()
    manifest=png.with_suffix(".json")
    if manifest.exists():
        atlas=json.loads(manifest.read_text(encoding="utf-8"))["textures"][0]
        frames=adapted_frames
        row["sourceFrames"]=original_frames
        row["runtimeFrames"]=frames
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
        row["metadataSHA256"]=hashlib.sha256(binary).hexdigest()
        row["frames"]=len(frames)
        row["frameRate"]=12 # ArenaBase.setBiome pinned source, not PokÃ©mon's 10 FPS.

header="// Generated pinned static arena layers.\n#pragma once\n#include \"content/ArenaTextures.hpp\"\nnamespace Pokerogue3DS {\nstruct ArenaLayerTextureDefinition { const char* key; const char* path; uint16_t width,height; const char* metadataPath; };\ninline constexpr ArenaLayerTextureDefinition kArenaLayerTextures[] = {\n"
header+="\n".join('    {"%s", "%s", %d, %d, %s},' % (r["key"],r["runtimePath"],r["width"],r["height"],json.dumps(r["metadataPath"]) if r["metadataPath"] else "nullptr") for r in layer_rows)
header+="\n};\n}\n"
(ROOT / "project/generated/include/content/ArenaLayerTextures.hpp").write_text(header,encoding="utf-8",newline="\n")
(output.parent / "layer-provenance.json").write_text(json.dumps({"repository":REPOSITORY,"revision":REVISION,"files":layer_rows,"unsupported":unsupported},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
(ROOT / "docs/generated/ARENA_LAYER_REPORT.json").write_bytes((json.dumps({"repository":REPOSITORY,"revision":REVISION,"files":layer_rows,"unsupported":unsupported,"runtimeValidation":"NOT_EXECUTED"},sort_keys=True,indent=2)+"\n").encode())
print(f"Converted {len(layer_rows)} arena layers ({sum(bool(row["metadataPath"]) for row in layer_rows)} animated); {len(unsupported)} unsupported")

