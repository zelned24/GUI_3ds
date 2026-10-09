"""Pinned native font sizes; no per-frame rasterization or fractional scaling."""
import hashlib
import json
import subprocess
import struct
from pathlib import Path
from pixel_font import compact_font, glyph_ink_bounds, font_ink_bounds, monochrome_font
from PIL import ImageFont, __version__ as pillow_version, features

POINTS=(8,10,12,16)
REPOSITORY="https://github.com/pagefaultgames/pokerogue-assets"
REVISION="056a1f408f26a3be4fef243f7462cb43608c7928"
SOURCE="fonts/pokemon-emerald-pro.ttf"

def prepare_fonts(root, whitelist):
    raw=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"show",REVISION+":"+SOURCE])
    source=root/"build/native-presentation/source"/SOURCE
    source.parent.mkdir(parents=True,exist_ok=True)
    source.write_bytes(raw)
    target=root/"build/romfs/presentation/fonts"
    target.mkdir(parents=True,exist_ok=True)
    codepoints=[int(token,16) for token in whitelist.read_text().split()]
    rows=[]
    for points in POINTS:
        name="emerald.bcfnt" if points==16 else f"emerald-{points}.bcfnt"
        path=target/name
        subprocess.run(["C:/devkitPro/tools/bin/mkbcfnt.exe","-s",str(points),"-w",str(whitelist),"-o",str(path),str(source)],check=True)
        original=path.read_bytes()
        font=ImageFont.truetype(str(source),points*96/72)
        def rasterize(cp):
            mask,offset=font.getmask2(chr(cp),mode="1",anchor="ls")
            return bytes(mask),mask.size,offset
        path.write_bytes(compact_font(monochrome_font(original,rasterize)))
        data=path.read_bytes()
        ink_top, ink_bottom = glyph_ink_bounds(data, ord('C'))
        text_top,text_bottom=font_ink_bounds(data,codepoints)
        glyph=struct.unpack_from("<I",data,36)[0]
        sheet_bytes=struct.unpack_from("<I",data,glyph+4)[0]*struct.unpack_from("<H",data,glyph+8)[0]
        rows.append({"textInkTop":text_top,"textInkHeight":text_bottom-text_top,"capitalInkTop":ink_top,"capitalInkHeight":ink_bottom-ink_top,"uncompactedBytes":len(original),"rasterCellHeight":data[glyph+1],"lineFeed":data[29],"sheetBytes":sheet_bytes,"points":points,"convertedPath":"romfs:/presentation/fonts/"+name,"convertedSHA256":hashlib.sha256(path.read_bytes()).hexdigest(),"bytes":path.stat().st_size})
    report={"repository":REPOSITORY,"revision":REVISION,"sourcePath":SOURCE,"sourceSHA256":hashlib.sha256(raw).hexdigest(),"schemaVersion":1,"whitelistSHA256":hashlib.sha256(whitelist.read_bytes()).hexdigest(),"alphaPolicy":"FreeType monochrome outlines at 96 DPI; native CFNT metrics","glyphMetricsPolicy":"monochrome ink width and left bearing; mkbcfnt advance and baseline preserved","rasterizer":{"pillow":pillow_version,"freetype":features.version("freetype2")},"files":rows}
    (root/"build/native-presentation/font-provenance.json").write_text(json.dumps(report,sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
    (root/"docs/generated/NATIVE_FONT_REPORT.json").write_bytes((json.dumps(report,sort_keys=True,indent=2)+"\n").encode("utf-8"))
    header=root/"project/generated/include/content/NativeFontMetrics.hpp"
    header.parent.mkdir(parents=True,exist_ok=True)
    header.write_text('#pragma once\n// Generated from pinned native BCFNT whitelist ink; preserve accents/baselines.\nnamespace Pokerogue3DS {\ninline constexpr unsigned kNativeFontInkTop[]={'+','.join(str(r['textInkTop']) for r in rows)+'};\ninline constexpr unsigned kNativeFontInkHeight[]={'+','.join(str(r['textInkHeight']) for r in rows)+'};\n}\n',encoding='utf-8',newline='\n')
    return report

if __name__=="__main__":
    root=Path(__file__).resolve().parents[1]
    prepare_fonts(root,root/"build/native-presentation/font-codepoints.txt")
