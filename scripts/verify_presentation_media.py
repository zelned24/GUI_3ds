#!/usr/bin/env python3
"""
Verification script for M2B: Assets & Presentation Media (Items, Trainers, Intro Cinematic).
Tests:
1. Physical item textures and generated reference tables; not runtime effect coverage.
2. RomFS item texture files and atlas.
3. Physical trainer sprites, metadata (.p3a), and generated type mapping tables.
4. Intro cinematic sequence texture and keyframe data.
"""

import os
import sys
import struct
import json
import hashlib
import re
from pathlib import Path

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ROMFS = os.path.join(ROOT, "build", "romfs")

def test_items():
    print("--- [1/3] Testing Item Sprites & Atlas ---")
    items_t3x = os.path.join(ROMFS, "presentation", "items", "items.t3x")
    items_0_t3x = os.path.join(ROMFS, "presentation", "items", "items-0.t3x")
    ui_items_0 = os.path.join(ROMFS, "presentation", "ui", "items-0.t3x")

    assert os.path.isfile(items_t3x), f"Missing {items_t3x}"
    assert os.path.isfile(items_0_t3x), f"Missing {items_0_t3x}"
    assert os.path.isfile(ui_items_0), f"Missing {ui_items_0}"

    size_atlas = os.path.getsize(items_t3x)
    assert size_atlas > 100000, f"Atlas too small: {size_atlas} bytes"
    print(f"  [OK] Atlas texture verified: {items_t3x} ({size_atlas:,} bytes)")

    loose_items_dir = os.path.join(ROMFS, "presentation", "items")
    loose_files = [f for f in os.listdir(loose_items_dir) if f.endswith(".t3x") and f not in ("items.t3x", "items-0.t3x")]
    assert len(loose_files) >= 500, f"Too few loose item textures: {len(loose_files)}"
    print(f"  [OK] Loose item textures verified: {len(loose_files)} items in {loose_items_dir}")

    # Check ItemIconReferences.hpp
    refs_header = os.path.join(ROOT, "project", "generated", "include", "content", "ItemIconReferences.hpp")
    assert os.path.isfile(refs_header), f"Missing {refs_header}"
    with open(refs_header, "r", encoding="utf-8") as f:
        refs_content = f.read()
    assert "kItemIconReferences" in refs_content
    assert "findItemIconKey" in refs_content
    print("  [OK] ItemIconReferences.hpp verified")

    # Check ItemIcons.hpp
    icons_header = os.path.join(ROOT, "project", "generated", "include", "content", "ItemIcons.hpp")
    assert os.path.isfile(icons_header), f"Missing {icons_header}"
    with open(icons_header, "r", encoding="utf-8") as f:
        icons_content = f.read()
    assert "kItemIconFrames" in icons_content
    assert "kItemIconPages" in icons_content
    print("  [OK] ItemIcons.hpp verified")
    print("  [OK] ALL item tests PASSED!\n")

def test_trainers():
    print("--- [2/3] Testing Trainer Sprites & Metadata ---")
    trainers_dir = os.path.join(ROMFS, "presentation", "trainers")
    assert os.path.isdir(trainers_dir), f"Missing {trainers_dir}"

    t3x_files = [f for f in os.listdir(trainers_dir) if f.endswith(".t3x")]
    p3a_files = [f for f in os.listdir(trainers_dir) if f.endswith(".p3a")]

    assert len(t3x_files) >= 300, f"Expected >= 300 trainer .t3x files, found {len(t3x_files)}"
    assert len(p3a_files) >= 300, f"Expected >= 300 trainer .p3a files, found {len(p3a_files)}"
    print(f"  [OK] Trainer textures: {len(t3x_files)} .t3x files in {trainers_dir}")
    print(f"  [OK] Trainer metadata: {len(p3a_files)} .p3a files in {trainers_dir}")

    # Check every record against pinned source; file presence is insufficient.
    provenance = json.loads((Path(ROOT)/"build/native-presentation/trainer-provenance.json").read_text(encoding="utf-8"))
    assert provenance["revision"] == "056a1f408f26a3be4fef243f7462cb43608c7928"
    omitted_count = 0
    for row in provenance["files"]:
        if not row["metadataPath"]:
            continue
        source = Path(ROOT)/"build/native-presentation/source"/row["sourcePath"]
        manifest = source.with_suffix(".json")
        raw_frames = json.loads(manifest.read_text(encoding="utf-8"))["textures"][0]["frames"]
        frames = [f for f in raw_frames if re.fullmatch(r"[A-Za-z0-9_.-]{1,11}",f["filename"])]
        omitted = [f for f in raw_frames if f not in frames]
        assert row["extensions"]["unrepresentedFrames"] == omitted
        omitted_count += len(omitted)
        binary = (Path(trainers_dir)/(row["key"]+".p3a")).read_bytes()
        assert binary[:8] == b"P3ATLAS1"
        assert binary[20:52] == hashlib.sha256(source.read_bytes()).digest()
        assert binary[52:84] == hashlib.sha256(manifest.read_bytes()).digest()
        assert len(binary) == 84+len(frames)*32
        assert struct.unpack_from("<I",binary,16)[0] == len(frames) == row["frameCount"]
        for index,frame in enumerate(frames):
            record = struct.unpack_from("<12s10H",binary,84+index*32)
            assert record[0].split(b"\0",1)[0].decode("ascii") == frame["filename"]
            bounds,source_size,trim = frame["frame"],frame["sourceSize"],frame["spriteSourceSize"]
            assert record[1:9] == (bounds["x"],bounds["y"],bounds["w"],bounds["h"],source_size["w"],source_size["h"],trim["x"],trim["y"])
            # Mirror native geometry preconditions, including its one-pixel trim tolerance.
            x,y,w,h,sw,sh,tx,ty,duration,flags = record[1:]
            assert w and h and sw and sh and not (flags & ~1), (row["key"],frame["filename"])
            assert x+w<=row["width"] and y+h<=row["height"], (row["key"],frame["filename"])
            assert tx+w<=sw+1 and ty+h<=sh+1, (row["key"],frame["filename"])

    print(f"  [OK] All trainer metadata records match source; {omitted_count} extended records preserved in provenance")

    # Verify .p3a binary header on sample files
    sample_p3a = os.path.join(trainers_dir, "blue.p3a")
    assert os.path.isfile(sample_p3a)
    with open(sample_p3a, "rb") as f:
        header = f.read(84)
        assert header[:8] == b"P3ATLAS1", f"Invalid p3a magic: {header[:8]}"
        version = struct.unpack("<I", header[8:12])[0]
        assert version == 1, f"Unexpected version: {version}"
        w, h = struct.unpack("<HH", header[12:16])
        frame_count = struct.unpack("<I", header[16:20])[0]
        assert frame_count > 10, f"Expected animated frames for Blue, found {frame_count}"
        print(f"  [OK] Verified blue.p3a: {w}x{h}, {frame_count} frames")

    sample_p3a_2 = os.path.join(trainers_dir, "trainer_m_back.p3a")
    assert os.path.isfile(sample_p3a_2)
    with open(sample_p3a_2, "rb") as f:
        header = f.read(84)
        assert header[:8] == b"P3ATLAS1"
        w, h = struct.unpack("<HH", header[12:16])
        frame_count = struct.unpack("<I", header[16:20])[0]
        print(f"  [OK] Verified trainer_m_back.p3a: {w}x{h}, {frame_count} frames")

    # Check TrainerSprites.hpp
    sprites_header = os.path.join(ROOT, "project", "generated", "include", "content", "TrainerSprites.hpp")
    assert os.path.isfile(sprites_header), f"Missing {sprites_header}"
    with open(sprites_header, "r", encoding="utf-8") as f:
        sprites_content = f.read()
    assert "kTrainerSprites" in sprites_content
    assert "kTrainerTypeSpriteMaps" in sprites_content
    assert "findTrainerSprite" in sprites_content
    assert "findPlayerBackSprite" in sprites_content
    print("  [OK] TrainerSprites.hpp verified")
    print("  [OK] ALL trainer tests PASSED!\n")

def test_intro_cinematic():
    import cv2
    import numpy as np
    from PIL import Image
    root=Path(ROOT)
    report=json.loads((root/"build/native-presentation/intro-provenance.json").read_text(encoding="utf-8"))
    assert report==json.loads((root/"docs/generated/INTRO_PRESENTATION_REPORT.json").read_text(encoding="utf-8"))
    assert report["schemaVersion"]==2 and report["revision"]=="056a1f408f26a3be4fef243f7462cb43608c7928"
    source=root/"build/native-presentation/source"/report["sourcePath"]
    assert hashlib.sha256(source.read_bytes()).hexdigest()==report["sourceSHA256"]
    capture=cv2.VideoCapture(str(source));assert capture.isOpened()
    fps=capture.get(cv2.CAP_PROP_FPS);frames=[]
    try:
        while True:
            ok,frame=capture.read()
            if not ok: break
            frames.append(frame)
    finally: capture.release()
    assert len(frames)==report["sourceFrameCount"]==len(report["keyframes"])
    assert fps==report["sourceFrameRate"]
    assert report["durationMs"]==round(len(frames)*1000/fps)
    sheets=[]
    for page in report["pages"]:
        png=root/page["stagedPath"]
        texture=root/"build/romfs"/page["runtimePath"].removeprefix("romfs:/")
        assert hashlib.sha256(png.read_bytes()).hexdigest()==page["stagedSHA256"]
        assert hashlib.sha256(texture.read_bytes()).hexdigest()==page["convertedSHA256"]
        with Image.open(png) as image:
            assert image.size==(1024,512)
            sheets.append(image.convert("RGB"))
    header=(root/"project/generated/include/content/IntroCinematicData.hpp").read_text(encoding="utf-8")
    assert f"kIntroKeyframeCount = {len(frames)}" in header
    assert f"kIntroPageCount = {len(sheets)}" in header
    for index,record in enumerate(report["keyframes"]):
        assert record["sourceFrame"]==index and record["page"]<len(sheets)
        frame=frames[index];height,width,_=frame.shape
        left,top=(width-480)//2,(height-240)//2
        assert width>=480 and height>=240 and (record["width"],record["height"])==(200,100)
        expected=cv2.cvtColor(cv2.resize(frame[top:top+240,left:left+480],(200,100),
            interpolation=cv2.INTER_NEAREST),cv2.COLOR_BGR2RGB)
        actual=np.asarray(sheets[record["page"]].crop((record["x"],record["y"],record["x"]+200,record["y"]+100)))
        assert np.array_equal(actual,expected), f"Intro source pixels differ at frame {index}"
        assert record["timeMs"]==round(index*1000/fps)
        assert "{%d,%d,%d,%d,%d,%d}"%(record["timeMs"],record["x"],record["y"],200,100,record["page"]) in header
    print(f"  [OK] All {len(frames)} intro frames in {len(sheets)} pages: exact nearest pixels, timestamps and physical hashes")


def test_setup_background():
    from PIL import Image
    root=Path(ROOT)
    report=json.loads((root/"build/native-presentation/setup-provenance.json").read_text(encoding="utf-8"))
    assert report["revision"]=="056a1f408f26a3be4fef243f7462cb43608c7928"
    for row in report["files"]:
        source=root/"build/native-presentation/source"/row["sourcePath"]
        texture=root/"build/romfs"/row["runtimePath"].removeprefix("romfs:/")
        assert hashlib.sha256(source.read_bytes()).hexdigest()==row["sourceSHA256"]
        assert hashlib.sha256(texture.read_bytes()).hexdigest()==row["convertedSHA256"]
        if row["adaptation"]:
            a=row["adaptation"];staged=root/a["stagedPath"]
            assert a["runtimeSize"]==[400,225] and a["runtimeScale"]==1 and a["resampling"]=="NEAREST"
            assert hashlib.sha256(staged.read_bytes()).hexdigest()==a["stagedSHA256"]
            with Image.open(source) as original, Image.open(staged) as native:
                assert list(original.size)==a["sourceSize"] and native.size==(400,225)
                expected=original.convert("RGBA").resize((400,225),Image.Resampling.NEAREST)
                assert expected.tobytes()==native.convert("RGBA").tobytes()
    print("  [OK] Setup background: exact offline nearest 400x225 raster and physical hashes")


def test_windows():
    root=Path(ROOT)
    report=json.loads((root / "build/native-presentation/window-provenance.json").read_text(encoding="utf-8"))
    header=(root / "project/generated/include/content/WindowTexture.hpp").read_text(encoding="utf-8")
    ids=set()
    for row in report["files"]:
        assert row["id"] not in ids
        ids.add(row["id"])
        source=root / "build/native-presentation/source" / row["sourcePath"]
        converted=root / "build/romfs" / row["runtimePath"].removeprefix("romfs:/")
        assert hashlib.sha256(source.read_bytes()).hexdigest()==row["sourceSHA256"]
        assert hashlib.sha256(converted.read_bytes()).hexdigest()==row["convertedSHA256"]
        assert struct.unpack(">II",source.read_bytes()[16:24])==(24,24)
        assert '{%d, "%s", "%s"}' % (row["id"],row["symbol"],row["runtimePath"]) in header
    assert ids
    print(f"  [OK] {len(ids)} imported window styles: physical paths, hashes and generated IDs")

def test_type_labels():
    root=Path(ROOT)
    report=json.loads((root/"build/native-presentation/type-label-provenance.json").read_text(encoding="utf-8"))
    for row in report["sources"]:
        raw=(root/"build/native-presentation/source"/row["sourcePath"]).read_bytes()
        assert hashlib.sha256(raw).hexdigest()==row["sha256"]
    atlas=json.loads((root/"build/native-presentation/source/images/types_es-ES.json").read_text())["textures"][0]
    assert len(report["frames"])==len(atlas["frames"])
    header=(root/"project/generated/include/content/TypeLabels.hpp").read_text(encoding="utf-8")
    for f in atlas["frames"]:
        b=f["frame"];c=f["sourceSize"];t=f["spriteSourceSize"]
        values=[b["x"],b["y"],b["w"],b["h"],c["w"],c["h"],t["x"],t["y"]]
        assert {"key":f["filename"],"bounds":values} in report["frames"]
        assert '{"%s",{%s}}' % (f["filename"],','.join(map(str,values))) in header
    path=root/"build/romfs"/report["runtimePath"].removeprefix("romfs:/")
    assert hashlib.sha256(path.read_bytes()).hexdigest()==report["convertedSHA256"]
    print(f"  [OK] {len(report['frames'])} original localized type frames and physical hashes")

def test_hud_types():
    root=Path(ROOT)
    report=json.loads((root/"build/native-presentation/hud-type-provenance.json").read_text(encoding="utf-8"))
    header=(root/"project/generated/include/content/HudTypeIcons.hpp").read_text(encoding="utf-8")
    for row in report["files"]:
        for source in row["sources"]:
            raw=(root/"build/native-presentation/source"/source["sourcePath"]).read_bytes()
            assert hashlib.sha256(raw).hexdigest()==source["sha256"]
        if len(row["sources"])==2:
            atlas=json.loads((root/"build/native-presentation/source"/row["sources"][1]["sourcePath"]).read_text())["textures"][0]
        else:
            w,h=struct.unpack(">II",(root/"build/native-presentation/source"/row["sources"][0]["sourcePath"]).read_bytes()[16:24])
            atlas={"frames":[{"filename":"exp" if row["key"]=="overlay_exp" else ("owned" if row["key"]=="icon_owned" else row["key"]),"frame":{"x":0,"y":0,"w":w,"h":h},"sourceSize":{"w":w,"h":h},"spriteSourceSize":{"x":0,"y":0}}]}
        if row["sizeOverride"]:
            override=json.loads((root/"project/data/assets/presentation-overrides.json").read_text())["overrides"][row["sources"][1]["sourcePath"]]
            assert override==row["sizeOverride"] and override["declaredSize"]==atlas["size"]
            assert override["physicalSize"]=={"w":row["width"],"h":row["height"]}
        assert len(row["frames"])==len(atlas["frames"])
        for f in atlas["frames"]:
            b=f["frame"];c=f["sourceSize"];t=f["spriteSourceSize"]
            values=[b["x"],b["y"],b["w"],b["h"],c["w"],c["h"],t["x"],t["y"]]
            assert {"key":f["filename"],"bounds":values} in row["frames"]
            assert '{"%s",{%s}}' % (f["filename"],','.join(map(str,values))) in header
        path=root/"build/romfs"/row["runtimePath"].removeprefix("romfs:/")
        assert hashlib.sha256(path.read_bytes()).hexdigest()==row["convertedSHA256"]
    category=next(row for row in report["files"] if row["key"]=="categories")
    assert (category["width"],category["height"])==(84,11)
    assert category["upstreamSourceSymbol"]=="FightUiHandler.setup"
    assert category["frames"]==[{"key":key,"bounds":[x,0,28,11,28,11,0,0]} for key,x in [("physical",0),("special",28),("status",56)]]
    assert len(report["files"])==18
    print("  [OK] Eighteen original HUD sheets including digit/label assets: frame offsets, physical hashes and generated tables")

def test_pixel_fonts():
    from pixel_font import crisp_font, glyph_ink_bounds, font_ink_bounds
    root=Path(ROOT)
    report=json.loads((root/"build/native-presentation/font-provenance.json").read_text())
    source=root/"build/native-presentation/source"/report["sourcePath"]
    assert hashlib.sha256(source.read_bytes()).hexdigest()==report["sourceSHA256"]
    assert [row["points"] for row in report["files"]]==[8,10,12,16]
    codepoints=[int(token,16) for token in (root/"build/native-presentation/font-codepoints.txt").read_text().split()]
    ink_tops=[];ink_heights=[]
    for row in report["files"]:
        data=(root/"build/romfs"/row["convertedPath"].removeprefix("romfs:/")).read_bytes()
        assert len(data)==row["bytes"] and hashlib.sha256(data).hexdigest()==row["convertedSHA256"]
        assert crisp_font(data)==data
        top,bottom=glyph_ink_bounds(data,ord('C'))
        assert row["capitalInkTop"]==top and row["capitalInkHeight"]==bottom-top
        top,bottom=font_ink_bounds(data,codepoints)
        assert row["textInkTop"]==top and row["textInkHeight"]==bottom-top
        ink_tops.append(top);ink_heights.append(bottom-top)
        glyph=struct.unpack_from("<I",data,36)[0]
        assert row["rasterCellHeight"]==data[glyph+1] and row["lineFeed"]==data[29]
        assert row["sheetBytes"]==struct.unpack_from("<I",data,glyph+4)[0]*struct.unpack_from("<H",data,glyph+8)[0]
    header=(root/"project/generated/include/content/NativeFontMetrics.hpp").read_text()
    assert 'kNativeFontInkTop[]={'+','.join(map(str,ink_tops))+'};' in header
    assert 'kNativeFontInkHeight[]={'+','.join(map(str,ink_heights))+'};' in header
    print("  [OK] Four pinned font rasters: native ink/compiled metrics, binary alpha, hashes and physical sheets")

if __name__ == "__main__":
    test_items()
    test_trainers()
    test_intro_cinematic()
    test_setup_background()
    test_windows()
    test_type_labels()
    test_hud_types()
    test_pixel_fonts()
    print("==================================================")
    print("  ALL PRESENTATION MEDIA VERIFICATION TESTS PASS  ")
    print("==================================================")
