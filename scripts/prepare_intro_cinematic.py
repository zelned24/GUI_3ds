"""Package every pinned intro frame; no game compilation or emulator launch."""
import hashlib
import json
import subprocess
from pathlib import Path
import cv2
from PIL import Image

REVISION = "056a1f408f26a3be4fef243f7462cb43608c7928"
REPOSITORY = "https://github.com/pagefaultgames/pokerogue-assets"

def prepare(root):
    source = root / "build/native-presentation/source/images/intro_dark.mp4"
    pinned = subprocess.check_output(["git", "-C", str(root / "build/upstream/pokerogue-assets"),
        "show", REVISION+":images/intro_dark.mp4"])
    if source.read_bytes() != pinned:
        raise ValueError("Intro source differs from pinned Git object")
    cap = cv2.VideoCapture(str(source))
    if not cap.isOpened(): raise ValueError("Unable to decode pinned intro")
    fps = cap.get(cv2.CAP_PROP_FPS)
    if not 0 < fps <= 1000: raise ValueError("Invalid intro frame rate")
    frames = []
    try:
        while True:
            ok, frame = cap.read()
            if not ok: break
            frames.append(frame)
    finally: cap.release()
    if not frames: raise ValueError("Empty intro")
    duration = round(len(frames)*1000/fps)
    if not 0 < duration <= 65535: raise ValueError("Intro duration out of range")
    # A 200x100 raster maps to 400x200 at integer 2x. Twenty frames per
    # 1024x512 RGBA8 page keep one resident page at 2 MiB, six for this pin.
    cell_w,cell_h,columns,per_page = 200,100,5,20
    target_dir = root / "build/romfs/presentation/cinematics"
    staging = root / "build/native-presentation"
    target_dir.mkdir(parents=True,exist_ok=True)
    records,pages = [],[]
    for first in range(0,len(frames),per_page):
        page = first//per_page
        sheet = Image.new("RGBA",(1024,512),(0,0,0,255))
        for index in range(first,min(first+per_page,len(frames))):
            frame = frames[index]
            h,w,_ = frame.shape
            if w<480 or h<240: raise ValueError("Intro frame smaller than pinned crop")
            x0,y0 = (w-480)//2,(h-240)//2
            pixels=cv2.cvtColor(cv2.resize(frame[y0:y0+240,x0:x0+480],
                (cell_w,cell_h),interpolation=cv2.INTER_NEAREST),cv2.COLOR_BGR2RGB)
            local=index-first;x=(local%columns)*cell_w;y=(local//columns)*cell_h
            sheet.paste(Image.fromarray(pixels),(x,y))
            records.append({"sourceFrame":index,"timeMs":round(index*1000/fps),
                "page":page,"x":x,"y":y,"width":cell_w,"height":cell_h})
        png=staging/f"intro-page-{page}.png";sheet.save(png)
        texture=target_dir/f"intro-page-{page}.t3x"
        subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(texture),str(png)],
            check=True,stdout=subprocess.DEVNULL)
        pages.append({"runtimePath":f"romfs:/presentation/cinematics/intro-page-{page}.t3x",
            "stagedPath":f"build/native-presentation/intro-page-{page}.png",
            "convertedSHA256":hashlib.sha256(texture.read_bytes()).hexdigest(),
            "stagedSHA256":hashlib.sha256(png.read_bytes()).hexdigest(),
            "residentTextureBytes":1024*512*4})
    header="""// Generated complete pinned intro sequence; native raster enlarged at integer 2x.
#pragma once
#include <cstdint>
namespace Pokerogue3DS {
struct IntroKeyframe {uint16_t timeMs; uint16_t x,y,width,height,page;};
"""
    header+=f"inline constexpr uint16_t kIntroTotalDurationMs = {duration};\n"
    header+=f"inline constexpr uint16_t kIntroKeyframeCount = {len(records)};\n"
    header+=f"inline constexpr uint16_t kIntroPageCount = {len(pages)};\n"
    header+="inline constexpr const char* kIntroCinematicPaths[] = {\n"
    header+=''.join('    "'+p["runtimePath"]+'",\n' for p in pages)+"};\n"
    header+="inline constexpr IntroKeyframe kIntroKeyframes[] = {\n"
    header+=''.join("    {%d,%d,%d,%d,%d,%d},\n"%(r["timeMs"],r["x"],r["y"],r["width"],r["height"],r["page"]) for r in records)
    header+="};\n} // namespace Pokerogue3DS\n"
    (root/"project/generated/include/content/IntroCinematicData.hpp").write_text(header,encoding="utf-8",newline="\n")
    report={"schemaVersion":2,"repository":REPOSITORY,"revision":REVISION,
        "sourcePath":"images/intro_dark.mp4","sourceSHA256":hashlib.sha256(pinned).hexdigest(),
        "sourceFrameRate":fps,"sourceFrameCount":len(frames),"durationMs":duration,
        "adaptation":"All source frames; nearest 200x100 raster; integer 2x display; timestamp-held playback",
        "runtimeValidation":"NOT_EXECUTED","pages":pages,"keyframes":records,
        "memory":{"residentPageBytes":1024*512*4,"transitionTextureBytes":2*1024*512*4,
            "note":"Texture allocation estimate; CPU, allocator, GPU retirement and I/O peaks require native measurement"}}
    serialized=json.dumps(report,sort_keys=True,indent=2)+"\n"
    (staging/"intro-provenance.json").write_text(serialized,encoding="utf-8",newline="\n")
    (root/"docs/generated/INTRO_PRESENTATION_REPORT.json").write_text(serialized,encoding="utf-8",newline="\n")
    print(f"Packaged all {len(frames)} intro frames in {len(pages)} pages")

if __name__ == "__main__": prepare(Path(__file__).resolve().parents[1])
