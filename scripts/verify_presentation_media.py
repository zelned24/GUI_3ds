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

if __name__ == "__main__":
    test_items()
    test_trainers()
    test_intro_cinematic()
    test_windows()
    print("==================================================")
    print("  ALL PRESENTATION MEDIA VERIFICATION TESTS PASS  ")
    print("==================================================")
