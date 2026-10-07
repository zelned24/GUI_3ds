"""Explicit nearest-neighbor presentation override; upstream files stay intact."""
import argparse
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
EDGE=1024
REVISION="056a1f408f26a3be4fef243f7462cb43608c7928"
REPOSITORY="https://github.com/pagefaultgames/pokerogue-assets"
def sha(data): return hashlib.sha256(data).hexdigest()
def inside(relative):
    p=(ROOT/relative).resolve();p.relative_to(ROOT)
    if Path(relative).is_absolute() or ".." in Path(relative).parts: raise ValueError("Unsafe sprite path")
    return p

def adapt_atlas(image,raw,max_width,max_height):
    if raw[:8]!=b"P3ATLAS1" or struct.unpack_from("<I",raw,8)[0]!=1: raise ValueError("Expected source P3ATLAS1")
    width,height,count=struct.unpack_from("<HHI",raw,12)
    if image.size!=(width,height) or not 0<count<=1024 or len(raw)!=84+32*count: raise ValueError("Invalid source atlas")
    records=[];common_w=common_h=0;names=set()
    for i in range(count):
        offset=84+i*32;name=raw[offset:offset+12].split(b"\0",1)[0]
        x,y,w,h,sw,sh,tx,ty,duration,flags=struct.unpack_from("<10H",raw,offset+12)
        if not re.fullmatch(rb"[A-Za-z0-9_.-]{1,11}",name) or name in names or flags&~1 or not min(w,h,sw,sh) or x+w>width or y+h>height or tx+w>sw+1 or ty+h>sh+1: raise ValueError("Invalid source frame")
        if sw>EDGE or sh>EDGE: raise ValueError("Source frame canvas exceeds preprocessing safety budget")
        names.add(name);cw,ch=max(sw,tx+w),max(sh,ty+h)
        common_w=max(common_w,cw);common_h=max(common_h,ch)
        records.append((i,name,x,y,w,h,cw,ch,tx,ty,duration))
    if not 1<=max_width<=EDGE or not 1<=max_height<=EDGE: raise ValueError("Invalid native canvas budget")
    if common_w<=max_width and common_h<=max_height: return None
    numerator,denominator=(max_width,common_w) if max_width*common_h<=max_height*common_w else (max_height,common_h)
    target_w=max(1,common_w*numerator//denominator);target_h=max(1,common_h*numerator//denominator)
    pages=[Image.new("RGBA",(EDGE,EDGE))];metadata=bytearray(raw)
    metadata[:8]=b"P3ATLAS2";struct.pack_into("<IHH",metadata,8,2,EDGE,EDGE)
    cursor_x=cursor_y=row_h=0
    used_w=used_h=0
    for i,name,x,y,w,h,cw,ch,tx,ty,duration in sorted(records,key=lambda r:r[1]):
        canvas=Image.new("RGBA",(common_w,common_h))
        # Common bottom/center anchor prevents frame-dependent resizing/jitter.
        canvas.paste(image.crop((x,y,x+w,y+h)),((common_w-cw)//2+tx,common_h-ch+ty))
        resized=canvas.resize((target_w,target_h),Image.Resampling.NEAREST)
        bounds=resized.getchannel("A").getbbox() or (0,0,1,1)
        left,top,right,bottom=bounds;frame=resized.crop(bounds);fw,fh=frame.size
        if cursor_x+fw+1>EDGE: cursor_x=0;cursor_y+=row_h;row_h=0
        if cursor_y+fh+1>EDGE:
            pages.append(Image.new("RGBA",(EDGE,EDGE)));cursor_x=cursor_y=row_h=0
        if len(pages)>4: raise ValueError("Native sprite exceeds four runtime pages")
        pages[-1].paste(frame,(cursor_x,cursor_y))
        struct.pack_into("<10H",metadata,84+i*32+12,cursor_x,cursor_y,fw,fh,target_w,target_h,left,top,duration,1|((len(pages)-1)<<1))
        used_w=max(used_w,cursor_x+fw);used_h=max(used_h,cursor_y+fh)
        cursor_x+=fw+1;row_h=max(row_h,fh+1)
    page_w=page_h=8
    while page_w<used_w: page_w*=2
    while page_h<used_h: page_h*=2
    pages=[page.crop((0,0,page_w,page_h)) for page in pages]
    struct.pack_into("<HH",metadata,12,page_w,page_h)
    return pages,bytes(metadata),{"originalCanvas":[common_w,common_h],"runtimeCanvas":[target_w,target_h],"resampler":"nearest","anchor":"bottom-center common canvas"}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--tex3ds",default="tex3ds");args=parser.parse_args()
    policy_path=ROOT/"project/data/assets/presentation-overrides.json";policy_bytes=policy_path.read_bytes();policy=json.loads(policy_bytes)["pokemonNativePixels"]
    if policy["revision"]!=REVISION or policy["resampler"]!="nearest": raise ValueError("Invalid sprite override policy")
    for facing in ("front","back"):
        for dimension in ("width","height"):
            value=policy[facing][dimension]
            if type(value) is not int or not 1<=value<=EDGE: raise ValueError("Invalid native canvas policy")
    staged=json.loads((ROOT/"build/upstream-assets/staged-sprite-assets.json").read_text())
    if staged["revision"]!=REVISION or staged["repository"]!=REPOSITORY: raise ValueError("Source pin mismatch")
    report_path=ROOT/"build/upstream-assets/native-sprite-overrides.json"
    converter_sha=sha(Path(__file__).read_bytes())
    header=ROOT/"project/generated/include/content/NativeSpritePolicy.hpp"
    header.write_text("// Generated presentation canvas budgets from explicit overrides.\n#pragma once\nnamespace Pokerogue3DS {\n"+"\n".join(f"inline constexpr unsigned kNative{facing.title()}Canvas{dimension.title()}={policy[facing][dimension]};" for facing in ("front","back") for dimension in ("width","height"))+"\n}\n",encoding="utf-8",newline="\n")
    prior=json.loads(report_path.read_text()) if report_path.exists() else {}
    cached={r["atlasKey"]+":"+r["facing"]:r for r in prior.get("assets",[])} if prior.get("policySHA256")==sha(policy_bytes) and prior.get("converterSHA256")==converter_sha else {}
    report={"schemaVersion":1,"repository":REPOSITORY,"revision":REVISION,"policySHA256":sha(policy_bytes),"converterSHA256":converter_sha,"assets":[]}
    for asset in sorted(staged["assets"],key=lambda a:(a["facing"],a["atlasKey"])):
        key,facing=asset["atlasKey"],asset["facing"]
        if not re.fullmatch(r"[1-9][0-9]*(?:-[a-z0-9-]+)?",key) or facing not in ("front","back"): raise ValueError("Invalid sprite identity")
        if asset["romfsMetadataPath"]!=f"romfs/sprites/pokemon/atlas/{facing}/{key}.p3a": raise ValueError("Unexpected runtime metadata path")
        raw=inside(asset["metadataPath"]).read_bytes();png=inside(asset["sourcePath"]).read_bytes()
        if sha(raw)!=asset["metadataSha256"] or sha(png)!=asset["expectedSha256"].removeprefix("sha256:"): raise ValueError("Source hash mismatch")
        if raw[20:52].hex()!=sha(png) or raw[52:84].hex()!=asset["manifestSha256"]: raise ValueError("Source provenance mismatch")
        old=cached.get(key+":"+facing)
        if old and old["sourceSHA256"]==sha(png) and old["sourceMetadataSHA256"]==sha(raw):
            files=[{"path":old["metadataPath"],"sha256":old["metadataSha256"]}]+old["textures"]
            if all(inside(r["path"]).exists() and sha(inside(r["path"]).read_bytes())==r["sha256"] for r in files): report["assets"].append(old);continue
        with Image.open(inside(asset["sourcePath"])) as source: result=adapt_atlas(source.convert("RGBA"),raw,policy[facing]["width"],policy[facing]["height"])
        if result is None: continue
        pages,metadata,adjustment=result
        row={"atlasKey":key,"facing":facing,"sourceSHA256":sha(png),"sourceMetadataSHA256":sha(raw),"manifestSha256":asset["manifestSha256"],"adjustment":adjustment,"textures":[]}
        directory=ROOT/"build/upstream-assets/native-pages"/facing;directory.mkdir(parents=True,exist_ok=True)
        texture_dir=ROOT/"build/romfs/sprites/pokemon"/("back" if facing=="back" else "");texture_dir.mkdir(parents=True,exist_ok=True)
        for n,page in enumerate(pages):
            derived=directory/f"{key}-p{n}.png";texture=texture_dir/f"{key}-p{n}.t3x"
            page.save(derived,format="PNG",optimize=False)
            subprocess.run([args.tex3ds,"--atlas","-f","rgba4","-z","auto","-o",str(texture),str(derived)],check=True,capture_output=True)
            row["textures"].append({"path":str(texture.relative_to(ROOT)).replace("\\","/"),"sha256":sha(texture.read_bytes())})
        metadata_path=inside("build/"+asset["romfsMetadataPath"]);metadata_path.write_bytes(metadata)
        row.update(metadataPath=str(metadata_path.relative_to(ROOT)).replace("\\","/"),metadataSha256=sha(metadata))
        report["assets"].append(row)
        report_path.write_text(json.dumps(report,sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
        print(key+":"+facing+" native pixels",flush=True)
    report_path.write_text(json.dumps(report,sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
    print(str(len(report["assets"]))+" native sprite overrides",flush=True)

if __name__=="__main__": main()
