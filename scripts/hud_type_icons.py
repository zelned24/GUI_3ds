"""Original BattleInfo type/status/owned icons, without resampling."""
import hashlib,json,struct,subprocess
from pathlib import Path
from type_badges import REVISION,REPOSITORY,atlas_frames
NAMES=["pbinfo_player_type","pbinfo_player_type1","pbinfo_player_type2","pbinfo_enemy_type","pbinfo_enemy_type1","pbinfo_enemy_type2"]
def validate_size(atlas,width,height,path,png,manifest,overrides):
    if atlas["size"]=={"w":width,"h":height}: return None
    override=overrides.get(path)
    if not override or override["revision"]!=REVISION or override["sourceSHA256"]!=hashlib.sha256(manifest).hexdigest() or override["imageSHA256"]!=hashlib.sha256(png).hexdigest() or override["declaredSize"]!=atlas["size"] or override["physicalSize"]!={"w":width,"h":height}: raise ValueError("HUD atlas dimensions mismatch without matching explicit override")
    return override

def prepare(root):
    rows=[]
    overrides=json.loads((root/"project/data/assets/presentation-overrides.json").read_text(encoding="utf-8"))["overrides"]
    for name,sourceBase,hasManifest in [(n,"images/ui/"+n,True) for n in NAMES]+[("statuses_es-ES","images/statuses_es-ES",True),("icon_owned","images/ui/icon_owned",False),("overlay_hp","images/ui/overlay_hp",True),("overlay_hp_boss","images/ui/overlay_hp_boss",True),("overlay_exp","images/ui/overlay_exp",False),("numbers","images/ui/numbers",True),("numbers_red","images/ui/numbers_red",True)]+[(n,"images/ui/text_images/es-ES/battle_ui/"+n+"_es-ES",False) for n in ["overlay_lv","overlay_hp_label","overlay_hp_label_boss","overlay_exp_label"]]:
        sources=[];data=[]
        for ext in (["png","json"] if hasManifest else ["png"]):
            path=sourceBase+"."+ext
            raw=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"show",REVISION+":"+path])
            file=root/"build/native-presentation/source"/path;file.parent.mkdir(parents=True,exist_ok=True);file.write_bytes(raw)
            sources.append({"sourcePath":path,"sha256":hashlib.sha256(raw).hexdigest()});data.append(raw)
        w,h=struct.unpack(">II",data[0][16:24])
        atlas=json.loads(data[1])["textures"][0] if hasManifest else {"size":{"w":w,"h":h},"frames":[{"filename":"exp" if name=="overlay_exp" else ("owned" if name=="icon_owned" else name),"frame":{"x":0,"y":0,"w":w,"h":h},"sourceSize":{"w":w,"h":h},"spriteSourceSize":{"x":0,"y":0}}]}
        appliedOverride=validate_size(atlas,w,h,sourceBase+".json",data[0],data[1] if hasManifest else b"",overrides)
        frames=atlas_frames(atlas,w,h,allow_numbers=name in ["numbers","numbers_red"])
        target=root/"build/romfs/presentation/ui"/(name+".t3x");target.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(root/"build/native-presentation/source"/sources[0]["sourcePath"])],check=True)
        rows.append({"key":name,"sizeOverride":appliedOverride,"width":w,"height":h,"sources":sources,"runtimePath":"romfs:/presentation/ui/"+name+".t3x","convertedSHA256":hashlib.sha256(target.read_bytes()).hexdigest(),"frames":[{"key":key,"bounds":values} for key,values in frames],"upstreamSourcePath":"src/ui/battle-info/enemy-battle-info.ts" if name=="icon_owned" else ("src/ui/battle-info/player-battle-info.ts" if name=="overlay_exp" else "src/ui/battle-info/battle-info.ts"),"upstreamSourceSymbol":"EnemyBattleInfo.constructor" if name=="icon_owned" else ("BattleInfo.updateStatusIcon" if name=="statuses_es-ES" else ("PlayerBattleInfo.initInfo" if name=="overlay_exp" else ("BattleInfo.updateHpFrame" if name.startswith("overlay_hp") else "BattleInfo.setTypes")))})
    symbols={"numbers":("battle-info","BattleInfo.setLevelDisplay"),"numbers_red":("player-battle-info","PlayerBattleInfo.setLevelDisplay"),"overlay_lv":("battle-info","BattleInfo.constructor"),"overlay_hp_label":("battle-info","BattleInfo.constructor"),"overlay_hp_label_boss":("enemy-battle-info","EnemyBattleInfo.updateBossSegments"),"overlay_exp_label":("player-battle-info","PlayerBattleInfo.constructor")}
    for row in rows:
        if row["key"] in symbols:
            file,symbol=symbols[row["key"]]
            row["upstreamSourcePath"]="src/ui/battle-info/"+file+".ts";row["upstreamSourceSymbol"]=symbol
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
