"""Import the pinned starter shiny indicator atlas; asset conversion only."""
import hashlib,json,re,struct,subprocess
from pathlib import Path
from type_badges import REVISION,REPOSITORY,atlas_frames
ROOT=Path(__file__).resolve().parents[1]
GAME_REVISION="8555c08c823b856cbec4eb99ca84ea52a955836d"
def prepare(root=ROOT):
    source="src/sprites/variant.ts"
    game=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue"),"show",GAME_REVISION+":"+source])
    body=game.decode().split("export function getVariantTint",1)[1].split("export function getVariantIcon",1)[0]
    colors={int(i):int(rgb,16) for i,rgb in re.findall(r"case ([0-2]):\s*return (0x[0-9a-f]+);",body)}
    if sorted(colors)!=[0,1,2]: raise ValueError("Unsupported pinned variant tint declarations")
    enum_path="src/enums/variant-tier.ts"
    enum_raw=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue"),"show",GAME_REVISION+":"+enum_path])
    enum_script="import {PokerogueEnumParser} from './tools/js/data/PokerogueEnumParser.js'; const value=new PokerogueEnumParser().parseEnum("+json.dumps(enum_raw.decode())+",'VariantTier','src/enums/variant-tier.ts');console.log(JSON.stringify(Object.fromEntries(value.symbolToId)));"
    tiers=json.loads(subprocess.check_output(["node","--input-type=module","-e",enum_script],cwd=root))
    icon_body=game.decode().split("export function getVariantIcon",1)[1].split("export function clearVariantData",1)[0]
    symbols={int(i):symbol for i,symbol in re.findall(r"case ([0-2]):\s*return VariantTier\.([A-Z_]+);",icon_body)}
    if sorted(symbols)!=[0,1,2] or any(symbol not in tiers for symbol in symbols.values()): raise ValueError("Unsupported pinned variant icon mapping")
    icon_ids={variant:tiers[symbol] for variant,symbol in symbols.items()}
    paths=["images/ui/legacy/shiny_icons.png","images/ui/legacy/shiny_icons.json"]
    data=[subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"show",REVISION+":"+path]) for path in paths]
    width,height=struct.unpack(">II",data[0][16:24]);atlas=json.loads(data[1])["textures"][0]
    # Phaser JSONArray applies spriteSourceSize only when trimmed is true.
    normalized=json.loads(json.dumps(atlas))
    for frame in normalized['frames']:
        if frame.get('trimmed') is False:
            bounds=frame['frame']
            frame['sourceSize']={'w':bounds['w'],'h':bounds['h']}
            frame['spriteSourceSize']={'x':0,'y':0,'w':bounds['w'],'h':bounds['h']}
    frames=atlas_frames(normalized,width,height,allow_numbers=True)
    by_key=dict(frames)
    if len(by_key)!=3 or any(str(icon_ids[v]) not in by_key for v in range(3)): raise ValueError("Unsupported variant frames")
    frames=[(str(icon_ids[v]),by_key[str(icon_ids[v])]) for v in range(3)]
    png=root/"build/native-presentation/source"/paths[0];png.parent.mkdir(parents=True,exist_ok=True);png.write_bytes(data[0])
    target=root/"build/romfs/presentation/ui/shiny_icons.t3x";target.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(png)],check=True)
    header='// Generated pinned starter variant indicators.\n#pragma once\n#include "gfx/renderer2d.hpp"\nnamespace Pokerogue3DS {\ninline constexpr const char* kStarterVariantIconPath="romfs:/presentation/ui/shiny_icons.t3x";\ninline constexpr Renderer2D::AtlasFrame kStarterVariantIconFrames[]={\n'
    header=header.replace('inline constexpr Renderer2D::AtlasFrame',f'inline constexpr unsigned kStarterVariantIconWidth={width},kStarterVariantIconHeight={height};\ninline constexpr Renderer2D::AtlasFrame',1)
    header+='\n'.join('    {'+','.join(map(str,bounds))+'},' for _,bounds in frames)+'\n};\ninline constexpr uint32_t kStarterVariantIconTints[]={\n'
    for variant in range(3):
        rgb=colors[variant];abgr=0xff000000|((rgb&255)<<16)|(rgb&0xff00)|(rgb>>16)
        header+='    0x%08xu,\n'%abgr
    header+='};\n}\n'
    (root/"project/generated/include/content/StarterVariantIcons.hpp").write_text(header,encoding="utf-8",newline="\n")
    report={"schemaVersion":1,"repository":REPOSITORY,"revision":REVISION,
        "sources":[{"sourcePath":path,"sha256":hashlib.sha256(raw).hexdigest()} for path,raw in zip(paths,data)],
        "gameSource":{"repository":"https://github.com/pagefaultgames/pokerogue","revision":GAME_REVISION,"sourcePath":source,"sourceSymbol":"getVariantTint","sha256":hashlib.sha256(game).hexdigest()},
        "enumSource":{"sourcePath":enum_path,"revision":GAME_REVISION,"sha256":hashlib.sha256(enum_raw).hexdigest()},
        "iconIDs":icon_ids,"declaredSize":atlas["size"],"physicalSize":{"w":width,"h":height},
        "sizeNote":"Upstream declares single-frame size; all frame bounds validated against physical PNG.",
        "normalization":"Untrimmed frames ignore spriteSourceSize offsets, matching Phaser JSONArray.",
        "parserReference":"https://raw.githubusercontent.com/phaserjs/phaser/v3.90.0/src/textures/parsers/JSONArray.js",
        "runtimePath":"romfs:/presentation/ui/shiny_icons.t3x","convertedSHA256":hashlib.sha256(target.read_bytes()).hexdigest(),"frames":frames,"tints":colors}
    (root/"build/native-presentation/starter-variant-provenance.json").write_text(json.dumps(report,sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
    print("Three pinned starter variant indicator frames converted")
if __name__=="__main__": prepare()
