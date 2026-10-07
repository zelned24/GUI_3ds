"""Import localized type labels from the pinned physical atlas."""
import hashlib,json,struct,subprocess
from pathlib import Path
REVISION="056a1f408f26a3be4fef243f7462cb43608c7928"
REPOSITORY="https://github.com/pagefaultgames/pokerogue-assets"
def prepare(root):
    base=root/"build/native-presentation/source"
    paths=["images/types_es-ES.png","images/types_es-ES.json"]
    raw=[]
    for path in paths:
        data=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"show",REVISION+":"+path])
        target=base/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);raw.append(data)
    atlas=json.loads(raw[1])["textures"][0]
    width,height=struct.unpack(">II",raw[0][16:24])
    if atlas["size"]!={"w":width,"h":height}: raise ValueError("Type atlas dimensions mismatch")
    frames=[];keys=set()
    for f in sorted(atlas["frames"],key=lambda f:f["filename"]):
        key=f["filename"]
        if key in keys or not key.isascii() or not key.isalpha() or f.get("rotated"): raise ValueError("Invalid type frame")
        keys.add(key);b=f["frame"];s=f["sourceSize"];t=f["spriteSourceSize"]
        if min(b["w"],b["h"],s["w"],s["h"])<=0 or min(b["x"],b["y"],t["x"],t["y"])<0 or b["x"]+b["w"]>width or b["y"]+b["h"]>height or t["x"]+b["w"]>s["w"] or t["y"]+b["h"]>s["h"]: raise ValueError("Type frame outside atlas/canvas")
        frames.append((key,[b["x"],b["y"],b["w"],b["h"],s["w"],s["h"],t["x"],t["y"]]))
    target=root/"build/romfs/presentation/ui/types_es-ES.t3x";target.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(base/paths[0])],check=True)
    header='// Generated pinned Spanish type label atlas.\n#pragma once\n#include "gfx/renderer2d.hpp"\nnamespace Pokerogue3DS {\nstruct TypeLabelFrame {const char* key;Renderer2D::AtlasFrame frame;};\ninline constexpr const char* kTypeLabelPath="romfs:/presentation/ui/types_es-ES.t3x";\ninline constexpr unsigned kTypeLabelAtlasWidth=%d,kTypeLabelAtlasHeight=%d;\ninline constexpr TypeLabelFrame kTypeLabelFrames[]={\n' % (width,height)
    header+='\n'.join('    {"%s",{%s}},' % (key,','.join(map(str,values))) for key,values in frames)
    header+='\n};\ninline const TypeLabelFrame* findTypeLabel(const char* type) {if(!type || !*type) return nullptr;for(const auto& row:kTypeLabelFrames) {const char* a=type;const char* b=row.key;while(*a && *b && ((*a>=65 && *a<=90) ? *a+32 : *a)==*b) {++a;++b;}if(!*a && !*b) return &row;}return nullptr;}\n}\n'
    (root/"project/generated/include/content/TypeLabels.hpp").write_text(header,encoding="utf-8",newline="\n")
    report={"schemaVersion":1,"repository":REPOSITORY,"revision":REVISION,"locale":"es-ES","upstreamConsumer":{"repository":"https://github.com/pagefaultgames/pokerogue","revision":"8555c08c823b856cbec4eb99ca84ea52a955836d","sourcePath":"src/ui/containers/starter-summary.ts","sourceSymbol":"StarterSummary.setupPokemonPermanentInfoContainer"},"sources":[{"sourcePath":path,"sha256":hashlib.sha256(data).hexdigest()} for path,data in zip(paths,raw)],"runtimePath":"romfs:/presentation/ui/types_es-ES.t3x","convertedSHA256":hashlib.sha256(target.read_bytes()).hexdigest(),"frames":[{"key":key,"bounds":values} for key,values in frames]}
    (root/"build/native-presentation/type-label-provenance.json").write_text(json.dumps(report,sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
if __name__=="__main__": prepare(Path(__file__).resolve().parents[1])
