"""Restore pinned egg atlas frames to native canvases and convert physical T3X."""
import hashlib,io,json,subprocess
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
REVISION="056a1f408f26a3be4fef243f7462cb43608c7928"
def prepare(root=ROOT):
    staged=root/"build/native-presentation/egg-frames";staged.mkdir(parents=True,exist_ok=True)
    output=root/"build/romfs/presentation/eggs";output.mkdir(parents=True,exist_ok=True)
    rows=[]
    for atlas_key in ("egg_icons","egg"):
        def source(ext):
            return subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"show",REVISION+":images/egg/"+atlas_key+ext])
        png=source(".png");manifest=source(".json")
        atlas_data=json.loads(manifest)["textures"]
        if len(atlas_data)!=1: raise ValueError("Unsupported egg atlas pages")
        with Image.open(io.BytesIO(png)) as atlas:
            for frame in sorted(atlas_data[0]["frames"],key=lambda row:row["filename"]):
                if frame.get("rotated"): raise ValueError("Rotated egg frame")
                b=frame["frame"];trim=frame["spriteSourceSize"];size=frame["sourceSize"]
                if b["w"]!=trim["w"] or b["h"]!=trim["h"] or min(b.values())<0 or b["x"]+b["w"]>atlas.width or b["y"]+b["h"]>atlas.height:
                    raise ValueError("Invalid egg source bounds")
                if trim["x"]<0 or trim["y"]<0 or trim["x"]+trim["w"]>size["w"] or trim["y"]+trim["h"]>size["h"]:
                    raise ValueError("Invalid egg canvas bounds")
                key=frame["filename"]
                if not key or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789_" for c in key): raise ValueError("Invalid egg frame key")
                canvas=Image.new("RGBA",(size["w"],size["h"]))
                canvas.paste(atlas.convert("RGBA").crop((b["x"],b["y"],b["x"]+b["w"],b["y"]+b["h"])),(trim["x"],trim["y"]))
                filename=atlas_key+"-"+key
                physical=staged/(filename+".png");canvas.save(physical)
                target=output/(filename+".t3x")
                subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(physical)],check=True,stdout=subprocess.DEVNULL)
                rows.append(dict(atlas=atlas_key,key=key,sourcePath="images/egg/"+atlas_key+".png",manifestPath="images/egg/"+atlas_key+".json",sourceSHA256=hashlib.sha256(png).hexdigest(),manifestSHA256=hashlib.sha256(manifest).hexdigest(),frame=frame,width=canvas.width,height=canvas.height,runtimePath="romfs:/presentation/eggs/"+target.name,convertedSHA256=hashlib.sha256(target.read_bytes()).hexdigest(),stagedSHA256=hashlib.sha256(physical.read_bytes()).hexdigest()))
    header="// Generated pinned native egg frames.\n#pragma once\n#include <cstring>\n#include <cstdint>\nnamespace Pokerogue3DS {\nstruct EggTexture {const char* atlas;const char* key;const char* path;uint16_t width,height;};\ninline constexpr EggTexture kEggTextures[]={\n"
    header+="\n".join("    {"+json.dumps(r["atlas"])+","+json.dumps(r["key"])+","+json.dumps(r["runtimePath"])+f',{r["width"]},{r["height"]}' +"}," for r in rows)
    header+='\n};\ninline const EggTexture* findEggTexture(const char* atlas,const char* key) {if(!atlas || !key) return nullptr;for(const auto& row:kEggTextures) if(!std::strcmp(atlas,row.atlas) && !std::strcmp(key,row.key)) return &row;return nullptr;}\n}\n'
    (root/"project/generated/include/content/EggTextures.hpp").write_bytes(header.encode())
    report=dict(schemaVersion=1,repository="https://github.com/pagefaultgames/pokerogue-assets",revision=REVISION,files=rows)
    (root/"docs/generated/EGG_TEXTURE_REPORT.json").write_bytes((json.dumps(report,sort_keys=True,indent=2)+"\n").encode())
    return report
if __name__=="__main__": print("Converted",len(prepare()["files"]),"egg frames")
