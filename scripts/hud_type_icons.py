"""Original BattleInfo type/status/owned icons, without resampling."""
import hashlib,json,struct,subprocess
from pathlib import Path
from type_badges import REVISION,REPOSITORY,atlas_frames
NAMES=["pbinfo_player_type","pbinfo_player_type1","pbinfo_player_type2","pbinfo_enemy_type","pbinfo_enemy_type1","pbinfo_enemy_type2"]
def prepare(root):
    rows=[]
    for name,sourceBase,hasManifest in [(n,"images/ui/"+n,True) for n in NAMES]+[("statuses_es-ES","images/statuses_es-ES",True),("icon_owned","images/ui/icon_owned",False)]:
        sources=[];data=[]
        for ext in (["png","json"] if hasManifest else ["png"]):
            path=sourceBase+"."+ext
            raw=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"show",REVISION+":"+path])
            file=root/"build/native-presentation/source"/path;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(raw)
            sources.append({"sourcePath":path,"sha256":hashlib.sha256(raw).hexdigest()});data.append(raw)
        w,h=struct.unpack(">II",data[0][16:24])
        atlas=json.loads(data[1])["textures"][0] if hasManifest else {"size":{"w":w,"h":h},"frames":[{"filename":"owned","frame":{"x":0,"y":0,"w":w,"h":h},"sourceSize":{"w":w,"h":h},"spriteSourceSize":{"x":0,"y":0}}]}
        if atlas["size"]!={"w":w,"h":h}: raise ValueError("HUD type atlas dimensions mismatch")
        frames=atlas_frames(atlas,w,h)
        target=root/"build/romfs/presentation/ui"/(name+".t3x");target.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(root/"build/native-presentation/source"/sources[0]["sourcePath"])],check=True)
        rows.append({"key":name,"width":w,"height":h,"sources":sources,"runtimePath":"romfs:/presentation/ui/"+name+".t3x","convertedSHA256":hashlib.sha256(target.read_bytes()).hexdigest(),"frames":[{"key":key,"bounds":values} for key,values in frames],"upstreamSourcePath":"src/ui/battle-info/enemy-battle-info.ts" if name=="icon_owned" else "src/ui/battle-info/battle-info.ts","upstreamSourceSymbol":"EnemyBattleInfo.constructor" if name=="icon_owned" else ("BattleInfo.updateStatusIcon" if name=="statuses_es-ES" else "BattleInfo.setTypes")})
    header='// Generated pinned BattleInfo type and indicator atlases.\n#pragma once\n#include "content/TypeLabels.hpp"\n#include <cstring>\nnamespace Pokerogue3DS {\n'
    for i,row in enumerate(rows):
        header+='inline constexpr TypeLabelFrame kHudIconFrames%d[]={\n' % i
        header+='\n'.join('    {"%s",{%s}},' % (f["key"],','.join(map(str,f["bounds"]))) for f in row["frames"])+"\n};\n"
    header+='struct HudIconAtlas {const char* key;const char* path;unsigned width,height;const TypeLabelFrame* frames;unsigned count;};\ninline constexpr HudIconAtlas kHudIconAtlases[]={\n'
    header+='\n'.join('    {"%s","%s",%d,%d,kHudIconFrames%d,sizeof(kHudIconFrames%d)/sizeof(kHudIconFrames%d[0])},' % (row["key"],row["runtimePath"],row["width"],row["height"],i,i,i) for i,row in enumerate(rows))
    header+='\n};\ninline const TypeLabelFrame* findHudTypeFrame(unsigned index,const char* type) {if(index>=6) return nullptr;const auto* label=findTypeLabel(type);if(!label) return nullptr;const auto& atlas=kHudIconAtlases[index];for(unsigned i=0;i<atlas.count;++i) if(!std::strcmp(atlas.frames[i].key,label->key)) return &atlas.frames[i];return nullptr;}\ninline const TypeLabelFrame* findHudIndicator(unsigned index,const char* key) {if(index<6 || index>=sizeof(kHudIconAtlases)/sizeof(kHudIconAtlases[0]) || !key) return nullptr;const auto& atlas=kHudIconAtlases[index];for(unsigned i=0;i<atlas.count;++i) if(!std::strcmp(atlas.frames[i].key,key)) return &atlas.frames[i];return nullptr;}\n}\n'
    (root/"project/generated/include/content/HudTypeIcons.hpp").write_text(header,encoding="utf-8",newline="\n")
    report={"schemaVersion":1,"repository":REPOSITORY,"revision":REVISION,"files":rows,"upstreamConsumer":{"repository":"https://github.com/pagefaultgames/pokerogue","revision":"8555c08c823b856cbec4eb99ca84ea52a955836d","sourcePath":"src/ui/battle-info/battle-info.ts","sourceSymbol":"BattleInfo.setTypes"}}
    (root/"build/native-presentation/hud-type-provenance.json").write_text(json.dumps(report,sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
if __name__=="__main__": prepare(Path(__file__).resolve().parents[1])
