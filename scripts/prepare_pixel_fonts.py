"""Pinned native font sizes; no per-frame rasterization or fractional scaling."""
import hashlib
import json
import subprocess
import struct
from pathlib import Path
from pixel_font import crisp_font

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
    rows=[]
    for points in POINTS:
        name="emerald.bcfnt" if points==16 else f"emerald-{points}.bcfnt"
        path=target/name
        subprocess.run(["C:/devkitPro/tools/bin/mkbcfnt.exe","-s",str(points),"-w",str(whitelist),"-o",str(path),str(source)],check=True)
        path.write_bytes(crisp_font(path.read_bytes()))
        data=path.read_bytes()
        glyph=struct.unpack_from("<I",data,36)[0]
        sheet_bytes=struct.unpack_from("<I",data,glyph+4)[0]*struct.unpack_from("<H",data,glyph+8)[0]
        rows.append({"rasterCellHeight":data[glyph+1],"lineFeed":data[29],"sheetBytes":sheet_bytes,"points":points,"convertedPath":"romfs:/presentation/fonts/"+name,"convertedSHA256":hashlib.sha256(path.read_bytes()).hexdigest(),"bytes":path.stat().st_size})
    report={"repository":REPOSITORY,"revision":REVISION,"sourcePath":SOURCE,"sourceSHA256":hashlib.sha256(raw).hexdigest(),"schemaVersion":1,"whitelistSHA256":hashlib.sha256(whitelist.read_bytes()).hexdigest(),"alphaPolicy":"A4 threshold 8/15 to binary alpha; native raster metrics","files":rows}
    (root/"build/native-presentation/font-provenance.json").write_text(json.dumps(report,sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
    return report

if __name__=="__main__":
    root=Path(__file__).resolve().parents[1]
    prepare_fonts(root,root/"build/native-presentation/font-codepoints.txt")
