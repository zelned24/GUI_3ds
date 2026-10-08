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
    print("--- [3/3] Testing Intro Cinematic ---")
    intro_t3x = os.path.join(ROMFS, "presentation", "cinematics", "intro_sequence.t3x")
    assert os.path.isfile(intro_t3x), f"Missing {intro_t3x}"
    size_intro = os.path.getsize(intro_t3x)
    assert size_intro > 50000, f"Intro texture too small: {size_intro} bytes"
    print(f"  [OK] Intro cinematic texture verified: {intro_t3x} ({size_intro:,} bytes)")

    intro_header = os.path.join(ROOT, "project", "generated", "include", "content", "IntroCinematicData.hpp")
    assert os.path.isfile(intro_header), f"Missing {intro_header}"
    with open(intro_header, "r", encoding="utf-8") as f:
        intro_content = f.read()
    assert "kIntroKeyframes" in intro_content
    assert "kIntroTotalDurationMs" in intro_content
    assert "kIntroKeyframeCount" in intro_content
    print("  [OK] IntroCinematicData.hpp verified")
    # Verify actual source pixels, not just a nonempty texture/header.
    import cv2
    import numpy as np
    from PIL import Image
    root=Path(ROOT)
    provenance=json.loads((root/"build/native-presentation/intro-provenance.json").read_text(encoding="utf-8"))
    assert provenance["repository"]=="https://github.com/pagefaultgames/pokerogue-assets"
    assert provenance["revision"]=="056a1f408f26a3be4fef243f7462cb43608c7928"
    assert provenance["sourcePath"]=="images/intro_dark.mp4"
    source=root/"build/native-presentation/source"/provenance["sourcePath"]
    assert hashlib.sha256(source.read_bytes()).hexdigest()==provenance["sourceSHA256"]
    assert hashlib.sha256(Path(intro_t3x).read_bytes()).hexdigest()==provenance["convertedSHA256"]
    capture=cv2.VideoCapture(str(source));assert capture.isOpened()
    fps=capture.get(cv2.CAP_PROP_FPS);frames=[]
    try:
        while True:
            ok,frame=capture.read()
            if not ok: break
            frames.append(frame)
    finally:
        capture.release()
    assert len(frames)==provenance["sourceFrameCount"] and fps==provenance["sourceFrameRate"]
    assert provenance["durationMs"]==round((len(frames)-1)*1000/fps)
    indices=np.linspace(0,len(frames)-1,16,dtype=int)
    assert len(provenance["keyframes"])==len(indices)
    with Image.open(root/"build/native-presentation/intro_sheet.png") as source_sheet:
        sheet=source_sheet.convert("RGB")
    for index,record in zip(indices,provenance["keyframes"]):
        frame=frames[index];height,width,_=frame.shape
        left,top=(width-480)//2,(height-240)//2
        assert width>=480 and height>=240
        expected=cv2.cvtColor(cv2.resize(frame[top:top+240,left:left+480],
            (record["width"],record["height"]),interpolation=cv2.INTER_NEAREST),cv2.COLOR_BGR2RGB)
        actual=np.asarray(sheet.crop((record["x"],record["y"],record["x"]+record["width"],record["y"]+record["height"])))
        assert np.array_equal(actual,expected), "Intro pixels differ from nearest sampled source"
        assert record["timeMs"]==round(int(index)*1000/fps)
    print("  [OK] Sixteen source frames: exact nearest pixels, timestamps and pinned/converted hashes")
    print("  [OK] ALL intro cinematic tests PASSED!\n")

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
    assert len(report["files"])==17
    print("  [OK] Seventeen original HUD sheets including digit/label assets: frame offsets, physical hashes and generated tables")

def test_pixel_fonts():
    from pixel_font import crisp_font, glyph_ink_bounds
    root=Path(ROOT)
    report=json.loads((root/"build/native-presentation/font-provenance.json").read_text())
    source=root/"build/native-presentation/source"/report["sourcePath"]
    assert hashlib.sha256(source.read_bytes()).hexdigest()==report["sourceSHA256"]
    assert [row["points"] for row in report["files"]]==[8,10,12,16]
    ink_tops=[];ink_heights=[]
    for row in report["files"]:
        data=(root/"build/romfs"/row["convertedPath"].removeprefix("romfs:/")).read_bytes()
        assert len(data)==row["bytes"] and hashlib.sha256(data).hexdigest()==row["convertedSHA256"]
        assert crisp_font(data)==data
        top,bottom=glyph_ink_bounds(data,ord('C'))
        assert row["capitalInkTop"]==top and row["capitalInkHeight"]==bottom-top
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
    test_windows()
    test_type_labels()
    test_hud_types()
    test_pixel_fonts()
    print("==================================================")
    print("  ALL PRESENTATION MEDIA VERIFICATION TESTS PASS  ")
    print("==================================================")
