"""Stage original pinned presentation assets with deterministic provenance."""
import hashlib
import io
import json
import subprocess
import zipfile
import re
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = "https://github.com/pagefaultgames/pokerogue-assets"
REVISION = "056a1f408f26a3be4fef243f7462cb43608c7928"
repo = ROOT / "build/upstream/pokerogue-assets"
# One archive avoids spawning a Git process for each texture.
archive = subprocess.check_output(["git", "-C", str(repo), "archive", "--format=zip", REVISION,
    "images/ui", "images/arenas", "images/logo.png", "images/items.png", "images/items.json",
    "images/items", "images/trainer", "images/intro_dark.mp4", "fonts"])
output = ROOT / "build/native-presentation/source"
rows = []
with zipfile.ZipFile(io.BytesIO(archive)) as sources:
    for name in sorted(sources.namelist()):
        if name.endswith("/"): continue
        relative = Path(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("Invalid source asset path")
        data = sources.read(name)
        target = output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
        rows.append({"sourcePath": name, "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)})
output.mkdir(parents=True, exist_ok=True)
(output.parent / "source-provenance.json").write_text(json.dumps({
    "schemaVersion": 1, "repository": REPOSITORY, "revision": REVISION, "files": rows
}, sort_keys=True, indent=2) + "\n", encoding="utf-8", newline="\n")
print(f"Staged {len(rows)} original pinned presentation assets")

# Package actual biome backgrounds. The generated resolver only names converted files.
import struct
from prepare_arena_backgrounds import prepare as prepare_arena_backgrounds
converted = prepare_arena_backgrounds(ROOT, output, REPOSITORY, REVISION)
print(f"Converted {len(converted)} nearest arena backgrounds")

hud_rows=[]
for name in ("pbinfo_player", "pbinfo_enemy_mini", "pbinfo_enemy_boss"):
    png=output / "images/ui" / (name+".png")
    target=ROOT / "build/romfs/presentation/ui" / (name+".t3x")
    target.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(png)],check=True,stdout=subprocess.DEVNULL)
    width,height=struct.unpack(">II",png.read_bytes()[16:24])
    hud_rows.append({"key":name,"sourcePath":"images/ui/"+name+".png","sourceSHA256":hashlib.sha256(png.read_bytes()).hexdigest(),"width":width,"height":height,"runtimePath":"romfs:/presentation/ui/"+name+".t3x","convertedSHA256":hashlib.sha256(target.read_bytes()).hexdigest()})
header="// Generated from pinned UI assets.\n#pragma once\n#include \"content/ArenaTextures.hpp\"\nnamespace Pokerogue3DS {\ninline constexpr ArenaTextureDefinition kBattleHudTextures[] = {\n"
header+="\n".join('    {"%s", "%s", %d, %d},' % (r["key"],r["runtimePath"],r["width"],r["height"]) for r in hud_rows)
header+="\n};\n}\n"
(ROOT / "project/generated/include/content/BattleHudTextures.hpp").write_text(header,encoding="utf-8",newline="\n")
(output.parent / "hud-provenance.json").write_text(json.dumps({"repository":REPOSITORY,"revision":REVISION,"files":hud_rows},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")

font_source=output / "fonts/pokemon-emerald-pro.ttf"
font_target=ROOT / "build/romfs/presentation/fonts/emerald.bcfnt"
font_target.parent.mkdir(parents=True,exist_ok=True)
canonical=json.loads((ROOT / "project/data/pokerogue/canonical-content.json").read_text(encoding="utf-8"))["collections"]
characters=set(chr(i) for i in range(32,127))
def collect_text(value):
    if isinstance(value,str): characters.update(value)
    elif isinstance(value,dict):
        for nested in value.values(): collect_text(nested)
    elif isinstance(value,list):
        for nested in value: collect_text(nested)
for domain in canonical.values():
    for record in domain:
        for field in ("name","names","description","descriptions","text","value"):
            if field in record: collect_text(record[field])
characters.update("♀♂↑↓←→ÁÉÍÓÚáéíóúÑñÜü¿¡₽•×")
# Import menu namespaces and their glyphs from the pinned locale snapshot.
ui_locale_revision="23aea1cb0da5a0b15b836f3c243791591cc42303"
ui_strings={}
ui_sources=[]
def flatten_ui(prefix,value):
    if isinstance(value,str): ui_strings[prefix]=value;characters.update(value)
    elif isinstance(value,dict):
        for key,nested in value.items(): flatten_ui(prefix+":"+key,nested)
for namespace in ["menu","menu-ui-handler","game-stats-ui-handler","settings","game-mode","starter-select-ui-handler","command-ui-handler","pokeball","ability","move"]:
    source_path="es-ES/"+namespace+".json"
    raw=subprocess.check_output(["git","-C",str(ROOT / "build/upstream/pokerogue-locales"),"show",ui_locale_revision+":"+source_path])
    namespace_data=json.loads(raw)
    if namespace in ["ability","move"]:
        namespace_data={key:{"name":value["name"]} for key,value in namespace_data.items() if isinstance(value,dict) and "name" in value}
    flatten_ui(namespace,namespace_data)
    ui_sources.append({"sourcePath":source_path,"sourceSHA256":hashlib.sha256(raw).hexdigest()})
ui_header="// Generated pinned Spanish presentation namespaces.\n#pragma once\n#include <cstring>\nnamespace Pokerogue3DS {\nstruct RuntimeUiText {const char* key;const char* text;};\ninline constexpr RuntimeUiText kRuntimeUiTexts[]={\n"
ui_header+="\n".join("    {"+json.dumps(key)+","+json.dumps(value,ensure_ascii=False)+"}," for key,value in sorted(ui_strings.items()))
ui_header+="\n};\ninline const char* runtimeUiText(const char* key) {unsigned first=0,last=sizeof(kRuntimeUiTexts)/sizeof(kRuntimeUiTexts[0]);while(first<last) {const unsigned mid=first+(last-first)/2;const int order=std::strcmp(kRuntimeUiTexts[mid].key,key);if(!order) return kRuntimeUiTexts[mid].text;if(order<0) first=mid+1;else last=mid;}return key;}\n}\n"
(ROOT / "project/generated/include/content/RuntimeUiText.hpp").write_text(ui_header,encoding="utf-8",newline="\n")
(output.parent / "ui-locale-provenance.json").write_text(json.dumps({"repository":"https://github.com/pagefaultgames/pokerogue-locales","revision":ui_locale_revision,"schemaVersion":1,"files":ui_sources},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
codepoints=sorted(ord(ch) for ch in characters if ord(ch)>=32)
whitelist=output.parent / "font-codepoints.txt"
whitelist.write_text(" ".join(hex(cp) for cp in codepoints)+"\n",encoding="utf-8",newline="\n")
from prepare_pixel_fonts import prepare_fonts
prepare_fonts(ROOT,whitelist)

import prepare_arena_layers

logo_source=output / "images/logo.png"
logo_target=ROOT / "build/romfs/presentation/images/logo.t3x"
logo_target.parent.mkdir(parents=True,exist_ok=True)
subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(logo_target),str(logo_source)],check=True,stdout=subprocess.DEVNULL)
(output.parent / "logo-provenance.json").write_text(json.dumps({"repository":REPOSITORY,"revision":REVISION,"sourcePath":"images/logo.png","sourceSHA256":hashlib.sha256(logo_source.read_bytes()).hexdigest(),"runtimePath":"romfs:/presentation/images/logo.t3x","convertedSHA256":hashlib.sha256(logo_target.read_bytes()).hexdigest()},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")

# Import the style IDs from the pinned enum; require its corresponding physical PNG.
import re
GAME_REVISION="8555c08c823b856cbec4eb99ca84ea52a955836d"
style_source=subprocess.check_output(["git","-C",str(ROOT / "build/upstream/pokerogue"),"show",GAME_REVISION+":src/enums/ui-window-style.ts"],text=True)
styles=re.findall(r"^\s*([A-Z_]+):\s*(\d+),",style_source,re.MULTILINE)
if not styles: raise ValueError("No UiWindowStyle IDs imported")
window_rows=[]
for symbol,raw_id in styles:
    style_id=int(raw_id)
    window_source=output / f"images/ui/windows/window_{style_id}.png"
    window_target=ROOT / f"build/romfs/presentation/ui/window_{style_id}.t3x"
    window_target.parent.mkdir(parents=True,exist_ok=True)
    raw=window_source.read_bytes()
    if struct.unpack(">II",raw[16:24])!=(24,24): raise ValueError("Invalid nine-slice window dimensions")
    subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(window_target),str(window_source)],check=True,stdout=subprocess.DEVNULL)
    window_rows.append({"id":style_id,"symbol":symbol,"sourcePath":window_source.relative_to(output).as_posix(),"sourceSHA256":hashlib.sha256(raw).hexdigest(),"runtimePath":f"romfs:/presentation/ui/window_{style_id}.t3x","convertedSHA256":hashlib.sha256(window_target.read_bytes()).hexdigest()})
header="// Generated from pinned UiWindowStyle and physical 24x24 window assets.\n#pragma once\n#include <cstddef>\nnamespace Pokerogue3DS {\nstruct WindowTextureDefinition { unsigned id; const char* symbol; const char* path; };\ninline constexpr WindowTextureDefinition kWindowTextures[] = {\n"
header+="\n".join('    {%d, "%s", "%s"},' % (r["id"],r["symbol"],r["runtimePath"]) for r in window_rows)
header+="\n};\ninline constexpr unsigned kWindowBorder=8;\ninline constexpr std::size_t kWindowTextureCount=sizeof(kWindowTextures)/sizeof(kWindowTextures[0]);\ninline constexpr const char* kWindowTexturePath=kWindowTextures[0].path;\ninline const WindowTextureDefinition* findWindowTexture(unsigned id) { for(const auto& row:kWindowTextures) if(row.id==id) return &row; return nullptr; }\n}\n"
(ROOT / "project/generated/include/content/WindowTexture.hpp").write_text(header,encoding="utf-8",newline="\n")
(output.parent / "window-provenance.json").write_text(json.dumps({"repository":REPOSITORY,"revision":REVISION,"enumRepository":"https://github.com/pagefaultgames/pokerogue","enumRevision":GAME_REVISION,"enumSourcePath":"src/enums/ui-window-style.ts","enumSourceSymbol":"UiWindowStyle","enumSHA256":hashlib.sha256(style_source.encode()).hexdigest(),"files":window_rows},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")

# UI strings come from the pinned locale repository, not presentation literals.
locale_revision="23aea1cb0da5a0b15b836f3c243791591cc42303"
locale_path="es-ES/menu.json"
locale_raw=subprocess.check_output(["git","-C",str(ROOT / "build/upstream/pokerogue-locales"),"show",locale_revision+":"+locale_path])
locale=json.loads(locale_raw)
keys=["continue","newGame","loadGame","runHistory","settings"]
text_header="// Generated from pinned es-ES/menu.json.\n#pragma once\nnamespace Pokerogue3DS {\ninline constexpr const char* kTitleMenuLabels[]={\n"
text_header+="\n".join("    "+json.dumps(locale[key],ensure_ascii=False)+"," for key in keys)
text_header+="\n};\n}\n"
(ROOT / "project/generated/include/content/TitleMenuText.hpp").write_text(text_header,encoding="utf-8",newline="\n")
(output.parent / "title-locale-provenance.json").write_text(json.dumps({"repository":"https://github.com/pagefaultgames/pokerogue-locales","revision":locale_revision,"sourcePath":locale_path,"sourceSHA256":hashlib.sha256(locale_raw).hexdigest(),"schemaVersion":1,"keys":keys},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")

cursor_source=output / "images/ui/cursor.png"
cursor_target=ROOT / "build/romfs/presentation/ui/cursor.t3x"
subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(cursor_target),str(cursor_source)],check=True,stdout=subprocess.DEVNULL)
(output.parent / "cursor-provenance.json").write_text(json.dumps({"repository":REPOSITORY,"revision":REVISION,"sourcePath":"images/ui/cursor.png","sourceSHA256":hashlib.sha256(cursor_source.read_bytes()).hexdigest(),"runtimePath":"romfs:/presentation/ui/cursor.t3x","convertedSHA256":hashlib.sha256(cursor_target.read_bytes()).hexdigest()},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")

# Pack source icons without redrawing them. Identity is derived from canonical forms.
from PIL import Image
icon_archive=subprocess.check_output(["git","-C",str(repo),"archive","--format=zip",REVISION,"images/pokemon/icons"])
icon_records=[]
icon_missing=[]
icon_pages=[]
forms_by_species={}
for form in canonical["forms"]:
    forms_by_species.setdefault(form["speciesId"],[]).append(form)
with zipfile.ZipFile(io.BytesIO(icon_archive)) as icons:
    names=set(icons.namelist())
    for species in sorted(canonical["species"],key=lambda row:row["speciesId"]):
        dex=species["speciesId"]
        forms=sorted(forms_by_species.get(species["id"],[]),key=lambda row:row["extensions"]["upstreamFormIndex"])
        candidate_map={0:str(dex)}
        for form in forms:
            candidate_map[form["extensions"]["upstreamFormIndex"]]=Path(form["assetReference"]["sourcePath"]).stem
        candidates=sorted(candidate_map.items())
        for form_index,key in candidates:
            name=f"images/pokemon/icons/{species['generation']}/{key}.png"
            if name not in names:
                icon_missing.append({"dex":dex,"formIndex":form_index,"expectedSourcePath":name,"status":"MISSING_IN_PINNED_ASSETS"})
                continue
            raw=icons.read(name)
            image=Image.open(io.BytesIO(raw)).convert("RGBA")
            if image.width>40 or image.height>32:
                raise ValueError(f"Unsupported source icon dimensions: {name} {image.size}")
            index=len(icon_records);page=index//192;cell=index%192;x=(cell%12)*40;y=(cell//12)*32
            if page==len(icon_pages): icon_pages.append(Image.new("RGBA",(512,512)))
            icon_pages[page].paste(image,(x,y))
            icon_records.append({"dex":dex,"formIndex":form_index,"page":page,"x":x,"y":y,"width":image.width,"height":image.height,"sourcePath":name,"sourceSHA256":hashlib.sha256(raw).hexdigest()})
icon_target=ROOT / "build/romfs/presentation/icons"
icon_target.mkdir(parents=True,exist_ok=True)
for page,image in enumerate(icon_pages):
    png=output.parent / f"icons-{page}.png";image.save(png)
    target=icon_target / f"icons-{page}.t3x"
    subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(png)],check=True,stdout=subprocess.DEVNULL)
icon_header="// Generated pinned PokemonSpecies.getIconId references.\n#pragma once\n#include <cstdint>\nnamespace Pokerogue3DS {\nstruct PokemonIconDefinition {uint16_t dex,formIndex,page,x,y,width,height;};\ninline constexpr PokemonIconDefinition kPokemonIcons[]={\n"
icon_header+="\n".join("    {%d,%d,%d,%d,%d,%d,%d}," % tuple(row[key] for key in ["dex","formIndex","page","x","y","width","height"]) for row in icon_records)
icon_header+="\n};\ninline constexpr const char* kPokemonIconPages[]={\n"+"\n".join(json.dumps(f"romfs:/presentation/icons/icons-{i}.t3x")+"," for i in range(len(icon_pages)))+"\n};\n}\n"
(ROOT / "project/generated/include/content/PokemonIcons.hpp").write_text(icon_header,encoding="utf-8",newline="\n")
(output.parent / "icon-provenance.json").write_text(json.dumps({"schemaVersion":1,"repository":REPOSITORY,"revision":REVISION,"sourceSymbol":"PokemonSpecies.getIconId","files":icon_records,"missing":icon_missing,"pages":len(icon_pages)},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
from prepare_compact_icons import prepare as prepare_compact_icons
prepare_compact_icons(ROOT)
from prepare_appearance_icons import prepare as prepare_appearance_icons
prepare_appearance_icons(ROOT)
print(f"Packed {len(icon_records)} real icons in {len(icon_pages)} pages; {len(icon_missing)} missing references")

mode_header="// Generated canonical modes with pinned locale labels.\n#pragma once\nnamespace Pokerogue3DS {\nstruct FrontendModeDefinition {const char* id;const char* label;};\ninline constexpr FrontendModeDefinition kFrontendModes[]={\n"
for mode in canonical["gameModes"]:
    key="game-mode:"+mode["displayNameKey"].split(":",1)[1]
    if key not in ui_strings: raise ValueError("Missing mode locale "+key)
    mode_header+="    {"+json.dumps(mode["id"])+","+json.dumps(ui_strings[key],ensure_ascii=False)+"},\n"
mode_header+="};\n}\n"
(ROOT / "project/generated/include/content/FrontendModes.hpp").write_text(mode_header,encoding="utf-8",newline="\n")

setup_rows=[]
for key in ["starter_select_bg","starter_container_bg"]:
    source=output / ("images/ui/"+key+".png")
    target=ROOT / ("build/romfs/presentation/ui/"+key+".t3x")
    conversion_source=source
    adaptation=None
    if key=="starter_select_bg":
        conversion_source=output.parent / "starter-select-native.png"
        with Image.open(source) as image:
            source_size=list(image.size)
            image.convert("RGBA").resize((400,225),Image.Resampling.NEAREST).save(conversion_source)
        adaptation={"sourceSize":source_size,"runtimeSize":[400,225],
            "resampling":"NEAREST","runtimeScale":1,
            "stagedPath":"build/native-presentation/starter-select-native.png",
            "stagedSHA256":hashlib.sha256(conversion_source.read_bytes()).hexdigest()}
    subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target),str(conversion_source)],check=True,stdout=subprocess.DEVNULL)
    setup_rows.append({"sourcePath":"images/ui/"+key+".png","sourceSHA256":hashlib.sha256(source.read_bytes()).hexdigest(),"runtimePath":"romfs:/presentation/ui/"+key+".t3x","convertedSHA256":hashlib.sha256(target.read_bytes()).hexdigest(),"adaptation":adaptation})
(output.parent / "setup-provenance.json").write_text(json.dumps({"repository":REPOSITORY,"revision":REVISION,"schemaVersion":1,"files":setup_rows},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")

# Preserve source atlas frame geometry, including trim offsets.
item_manifest_source=output / "images/items.json"
item_manifest=json.loads(item_manifest_source.read_text(encoding="utf-8"))
items_romfs_dir = ROOT / "build/romfs/presentation/items"
items_romfs_dir.mkdir(parents=True, exist_ok=True)
item_frames=[]
for page,texture in enumerate(item_manifest["textures"]):
    source=output / "images" / texture["image"]
    if not source.is_file(): raise ValueError("Missing pinned item texture "+texture["image"])
    target_ui=ROOT / f"build/romfs/presentation/ui/items-{page}.t3x"
    target_items=items_romfs_dir / f"items-{page}.t3x"
    subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe","-f","rgba8","-o",str(target_ui),str(source)],check=True,stdout=subprocess.DEVNULL)
    target_items.write_bytes(target_ui.read_bytes())
    if page == 0:
        (items_romfs_dir / "items.t3x").write_bytes(target_ui.read_bytes())
    for frame in texture["frames"]:
        if frame.get("rotated"): raise ValueError("Unsupported rotated item frame "+frame["filename"])
        rect=frame["frame"];size=frame["sourceSize"];trim=frame["spriteSourceSize"]
        item_frames.append({"key":frame["filename"],"page":page,"x":rect["x"],"y":rect["y"],"width":rect["w"],"height":rect["h"],"sourceWidth":size["w"],"sourceHeight":size["h"],"trimX":trim["x"],"trimY":trim["y"]})

# Convert loose individual item icons from images/items into romfs presentation/items
loose_item_dir = output / "images/items"
if loose_item_dir.exists():
    for loose_png in sorted(loose_item_dir.glob("*.png")):
        loose_target = items_romfs_dir / (loose_png.stem + ".t3x")
        if not loose_target.exists():
            subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe", "-f", "rgba8", "-o", str(loose_target), str(loose_png)], check=True, stdout=subprocess.DEVNULL)

item_header="// Generated pinned items.json frames; retain original trim coordinates.\n#pragma once\n#include <cstdint>\nnamespace Pokerogue3DS {\nstruct ItemIconFrame {const char* key;uint16_t page,x,y,width,height,sourceWidth,sourceHeight,trimX,trimY;};\ninline constexpr ItemIconFrame kItemIconFrames[]= {\n"
item_header+="\n".join("    {"+json.dumps(row["key"])+","+",".join(str(row[key]) for key in ["page","x","y","width","height","sourceWidth","sourceHeight","trimX","trimY"])+"}," for row in sorted(item_frames,key=lambda row:row["key"]))
item_header+="\n};\ninline constexpr const char* kItemIconPages[]= {\n"+"\n".join(json.dumps(f"romfs:/presentation/items/items-{page}.t3x")+"," for page in range(len(item_manifest["textures"])))+"\n};\n}\n"
(ROOT / "project/generated/include/content/ItemIcons.hpp").write_text(item_header,encoding="utf-8",newline="\n")
(output.parent / "item-icon-provenance.json").write_text(json.dumps({"repository":REPOSITORY,"revision":REVISION,"sourcePath":"images/items.json","sourceSHA256":hashlib.sha256(item_manifest_source.read_bytes()).hexdigest(),"schemaVersion":1,"frames":item_frames},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
print(f"Imported {len(item_frames)} original item icon frames")

# Reuse the existing upstream enum parser for native ball IDs.
import re
source_revision="8555c08c823b856cbec4eb99ca84ea52a955836d"
ball_source=subprocess.check_output(["git","-C",str(ROOT / "build/upstream/pokerogue"),"show",source_revision+":src/data/pokeball.ts"])
ball_enum_script="import {execFileSync} from 'node:child_process'; import {PokerogueEnumParser} from './tools/js/data/PokerogueEnumParser.js'; const raw=execFileSync('git',['-C','build/upstream/pokerogue','show','8555c08c823b856cbec4eb99ca84ea52a955836d:src/enums/pokeball.ts'],{encoding:'utf8'}); const c=new PokerogueEnumParser().parseEnum(raw,'PokeballType','src/enums/pokeball.ts'); console.log(JSON.stringify(Object.fromEntries(c.symbolToId)));"
ball_ids=json.loads(subprocess.check_output(["node","--input-type=module","-e",ball_enum_script],cwd=ROOT))
ball_code=ball_source.decode("utf-8")
atlas_function=ball_code.split("export function getPokeballAtlasKey",1)[1].split("export function",1)[0]
name_function=ball_code.split("export function getPokeballName",1)[1].split("export function",1)[0]
ball_keys=dict(re.findall(r'case PokeballType\.(\w+):\s*return "([^"]+)"',atlas_function))
ball_locales=dict(re.findall(r'case PokeballType\.(\w+):\s*ret = i18next.t\("([^"]+)"',name_function))
ball_rows=[]
ball_unused=[]
ball_scene_source=subprocess.check_output(["git","-C",str(ROOT / "build/upstream/pokerogue"),"show",source_revision+":src/battle-scene.ts"])
unused_symbols=set(re.findall(r"\.filter\(pt => pt !== PokeballType\.(\w+)\)",ball_scene_source.decode("utf-8")))
for symbol,value in sorted(ball_ids.items(),key=lambda row:row[1]):
    if symbol in unused_symbols:
        ball_unused.append({"id":value,"symbol":symbol,"status":"UPSTREAM_UNUSED_IN_POKEBALL_COUNTS"})
        continue
    key=ball_keys[symbol];locale_key=ball_locales[symbol]
    if not any(frame["key"]==key for frame in item_frames): raise ValueError("Missing physical ball icon "+key)
    ball_rows.append({"id":value,"symbol":symbol,"iconKey":key,"localeKey":locale_key,"label":ui_strings[locale_key]})
ball_header="// Generated getPokeballAtlasKey/getPokeballName with parsed upstream enum IDs.\n#pragma once\n#include <cstdint>\nnamespace Pokerogue3DS {\nstruct BallMenuDefinition {uint8_t id;const char* iconKey;const char* label;};\ninline constexpr BallMenuDefinition kBallMenuDefinitions[]={\n"
ball_header+="\n".join("    {"+str(row["id"])+","+json.dumps(row["iconKey"])+","+json.dumps(row["label"],ensure_ascii=False)+"}," for row in ball_rows)+"\n};\n}\n"
(ROOT / "project/generated/include/content/BallMenuContent.hpp").write_text(ball_header,encoding="utf-8",newline="\n")
(output.parent / "ball-menu-provenance.json").write_text(json.dumps({"repository":"https://github.com/pagefaultgames/pokerogue","revision":source_revision,"sourcePath":"src/data/pokeball.ts","sourceSymbol":["getPokeballAtlasKey","getPokeballName"],"sourceSHA256":hashlib.sha256(ball_source).hexdigest(),"schemaVersion":1,"entries":ball_rows,"upstreamUnused":ball_unused,"inventorySourcePath":"src/battle-scene.ts","inventorySourceSHA256":hashlib.sha256(ball_scene_source).hexdigest()},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")

# Resolve only declarative literal iconImage arguments from inspected constructors.
modifier_source=subprocess.check_output(["git","-C",str(ROOT / "build/upstream/pokerogue"),"show",source_revision+":src/modifier/modifier-type.ts"]).decode("utf-8")
item_icon_refs=[]
item_icon_unknown=[]
for item in canonical["items"]:
    raw=item["extensions"]["upstreamRawRecord"]["value"]
    constructor=re.search(r"new\s+(\w+)\s*\(",raw)
    if not constructor:
        item_icon_unknown.append({"id":item["id"],"status":"NOT_YET_SUPPORTED_BY_ICON_ADAPTER"});continue
    class_name=constructor.group(1)
    class_start=re.search(r"\bclass\s+"+re.escape(class_name)+r"\b",modifier_source)
    signature=None
    if class_start:
        body=modifier_source[class_start.end():]
        next_class=re.search(r"\b(?:export\s+)?class\s+\w+",body)
        if next_class: body=body[:next_class.start()]
        signature=re.search(r"constructor\s*\(([^)]*)\)",body,re.S)
    if not signature or not re.search(r"\biconImage\s*:",signature.group(1)):
        item_icon_unknown.append({"id":item["id"],"constructor":class_name,"status":"NOT_YET_SUPPORTED_BY_ICON_ADAPTER"});continue
    parameters=signature.group(1).split(",")
    icon_index=next(i for i,param in enumerate(parameters) if re.search(r"\biconImage\s*:",param))
    arguments=raw[constructor.end():]
    literals=[]
    for index in range(icon_index+1):
        literal=re.match(r'\s*("(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\')',arguments)
        if not literal: break
        token=literal.group(1)
        literals.append(json.loads(token) if token.startswith('"') else token[1:-1])
        arguments=arguments[literal.end():].lstrip()
        if arguments.startswith(","): arguments=arguments[1:]
    if len(literals)<=icon_index:
        item_icon_unknown.append({"id":item["id"],"constructor":class_name,"status":"NOT_YET_SUPPORTED_BY_ICON_ADAPTER"});continue
    key=literals[icon_index]
    if not any(frame["key"]==key for frame in item_frames):
        item_icon_unknown.append({"id":item["id"],"iconKey":key,"status":"MISSING_IN_PINNED_ITEM_ATLAS"});continue
    item_icon_refs.append({"id":item["id"],"iconKey":key,"constructor":class_name,"iconArgumentIndex":icon_index,"sourceSymbol":item["source"]["sourceSymbol"]})

ref_header="// Generated literal upstream item icon references.\n#pragma once\n#include <cstring>\nnamespace Pokerogue3DS {\nstruct ItemIconReference {const char* itemId;const char* iconKey;};\ninline constexpr ItemIconReference kItemIconReferences[]={\n"
ref_header+="\n".join("    {"+json.dumps(row["id"])+","+json.dumps(row["iconKey"])+"}," for row in sorted(item_icon_refs,key=lambda row:row["id"]))+"\n};\n\n"
ref_header+="""inline const char* findItemIconKey(const char* itemId) {
    if (!itemId) return nullptr;
    for (const auto& item : kItemIconReferences) {
        if (std::strcmp(item.itemId, itemId) == 0) return item.iconKey;
    }
    return nullptr;
}
}
"""
(ROOT / "project/generated/include/content/ItemIconReferences.hpp").write_text(ref_header,encoding="utf-8",newline="\n")
(output.parent / "item-icon-reference-report.json").write_text(json.dumps({"repository":"https://github.com/pagefaultgames/pokerogue","revision":source_revision,"sourcePath":"src/modifier/modifier-type.ts","sourceSHA256":hashlib.sha256(modifier_source.encode("utf-8")).hexdigest(),"schemaVersion":1,"resolved":item_icon_refs,"unsupported":item_icon_unknown},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
print(f"Resolved {len(item_icon_refs)} item icons; {len(item_icon_unknown)} explicit dynamic/unsupported references")

entity_header="// Canonical numeric IDs to pinned Spanish names; no gameplay behavior.\n#pragma once\n#include <cstdint>\nnamespace Pokerogue3DS {\nstruct EntityUiName {uint16_t id;const char* name;};\n"
entity_unknown=[]
entity_enum_script="import {execFileSync} from 'node:child_process'; import {PokerogueEnumParser} from './tools/js/data/PokerogueEnumParser.js'; const result={}; for(const [name,path] of [['AbilityId','src/enums/ability-id.ts'],['MoveId','src/enums/move-id.ts']]) {const raw=execFileSync('git',['-C','build/upstream/pokerogue','show','8555c08c823b856cbec4eb99ca84ea52a955836d:'+path],{encoding:'utf8'});const c=new PokerogueEnumParser().parseEnum(raw,name,path);result[name]=Object.fromEntries(c.idToSymbol);}console.log(JSON.stringify(result));"
entity_symbols=json.loads(subprocess.check_output(["node","--input-type=module","-e",entity_enum_script],cwd=ROOT))
for domain,namespace,id_field,enum_name in [("abilities","ability","abilityId","AbilityId"),("moves","move","moveId","MoveId")]:
    name_rows=[]
    for record in sorted(canonical[domain],key=lambda row:row[id_field]):
        raw=record["extensions"]["upstreamRawRecord"]["value"]
        symbol_match=re.search(enum_name+r"\.(\w+)",raw)
        symbol=entity_symbols[enum_name].get(str(record[id_field]))
        if symbol is None: raise ValueError("Canonical entity ID absent from upstream enum: "+str(record[id_field]))
        words=symbol.split("_")
        locale_key=namespace+":"+words[0].lower()+"".join(word[:1].upper()+word[1:].lower() for word in words[1:])+":name"
        value=ui_strings.get(locale_key)
        if value is None:
            value=record["name"]
            entity_unknown.append({"domain":domain,"id":record[id_field],"localeKey":locale_key,"status":"MISSING_IN_PINNED_LOCALE"})
        name_rows.append({"id":record[id_field],"name":value})
    table="kAbilityUiNames" if domain=="abilities" else "kMoveUiNames"
    entity_header+="inline constexpr EntityUiName "+table+"[]={\n"+"\n".join("    {"+str(row["id"])+","+json.dumps(row["name"],ensure_ascii=False)+"}," for row in name_rows)+"\n};\n"
entity_header+="template<unsigned N> inline const char* entityUiName(const EntityUiName (&rows)[N],uint16_t id) {unsigned first=0,last=N;while(first<last){const unsigned mid=first+(last-first)/2;if(rows[mid].id==id)return rows[mid].name;if(rows[mid].id<id)first=mid+1;else last=mid;}return nullptr;}\ninline const char* abilityUiName(uint16_t id) {return entityUiName(kAbilityUiNames,id);}\ninline const char* moveUiName(uint16_t id) {return entityUiName(kMoveUiNames,id);}\n}\n"
(ROOT / "project/generated/include/content/EntityUiNames.hpp").write_text(entity_header,encoding="utf-8",newline="\n")
(output.parent / "entity-locale-report.json").write_text(json.dumps({"repository":"https://github.com/pagefaultgames/pokerogue-locales","revision":ui_locale_revision,"sourceSymbol":["Ability.name","Move.constructor","toCamelCase"],"schemaVersion":1,"missing":entity_unknown},sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
print(f"Localized canonical move/ability IDs; {len(entity_unknown)} explicit missing locale names")

# ==============================================================================
# TRAINER SPRITES & METADATA PACKAGING
# ==============================================================================
trainer_source_dir = output / "images/trainer"
trainers_romfs_dir = ROOT / "build/romfs/presentation/trainers"
trainers_romfs_dir.mkdir(parents=True, exist_ok=True)

trainer_pngs = sorted(trainer_source_dir.glob("*.png"))
print(f"Packaging {len(trainer_pngs)} trainer sprites into RomFS...")

def convert_trainer_texture(png_path):
    target = trainers_romfs_dir / (png_path.stem + ".t3x")
    subprocess.run(["C:/devkitPro/tools/bin/tex3ds.exe", "-f", "rgba8", "-o", str(target), str(png_path)],
                   check=True, stdout=subprocess.DEVNULL)

with ThreadPoolExecutor(max_workers=8) as executor:
    list(executor.map(convert_trainer_texture, trainer_pngs))

trainer_rows = []
for png in trainer_pngs:
    key = png.stem
    target = trainers_romfs_dir / (key + ".t3x")
    raw = png.read_bytes()
    width, height = struct.unpack(">II", raw[16:24])
    row = {
        "key": key,
        "sourcePath": png.relative_to(output).as_posix(),
        "sourceSHA256": hashlib.sha256(raw).hexdigest(),
        "width": width,
        "height": height,
        "runtimePath": f"romfs:/presentation/trainers/{key}.t3x",
        "convertedSHA256": hashlib.sha256(target.read_bytes()).hexdigest(),
        "metadataPath": None,
        "frameCount": 1
    }
    manifest = png.with_suffix(".json")
    if manifest.exists():
        manifest_data = json.loads(manifest.read_text(encoding="utf-8"))
        textures = manifest_data.get("textures", [])
        if textures and textures[0].get("frames"):
            raw_frames = textures[0]["frames"]
            # P3ATLAS1 has an 11-byte safe filename field. Preserve unsupported
            # names and full records in provenance instead of truncating them.
            frames = [f for f in raw_frames if re.fullmatch(r"[A-Za-z0-9_.-]{1,11}", f["filename"])]
            row["extensions"] = {"unrepresentedFrames": [f for f in raw_frames if f not in frames]}
            if not frames:
                raise ValueError(f"No runtime-compatible trainer frames: {key}")
            binary = bytearray(struct.pack("<8sIHHI", b"P3ATLAS1", 1, width, height, len(frames)))
            binary.extend(hashlib.sha256(raw).digest())
            binary.extend(hashlib.sha256(manifest.read_bytes()).digest())
            for frame in frames:
                bounds = frame["frame"]
                source_sz = frame["sourceSize"]
                trim = frame["spriteSourceSize"]
                fname = frame["filename"].encode("ascii")
                binary.extend(struct.pack("<12s10H", fname, bounds["x"], bounds["y"], bounds["w"], bounds["h"],
                                          source_sz["w"], source_sz["h"], trim["x"], trim["y"], 0, 0))
            meta_path = trainers_romfs_dir / (key + ".p3a")
            meta_path.write_bytes(binary)
            row["metadataPath"] = f"romfs:/presentation/trainers/{key}.p3a"
            row["frameCount"] = len(frames)
    trainer_rows.append(row)

trainer_keys_set = {r["key"] for r in trainer_rows}

# Map canonical trainer types to authentic trainer sprite keys
trainer_mappings = []
for t in canonical["trainers"]:
    tid = t["trainerTypeId"]
    raw_key = t["id"].lower()
    male_candidate = None
    female_candidate = None
    candidates_m = [raw_key, f"{raw_key}_m", f"trainer_{raw_key}"]
    candidates_f = [f"{raw_key}_f", f"{raw_key}female", f"trainer_{raw_key}_f"]
    if raw_key == "rival":
        candidates_m = ["rival_m", "rival_f"]
        candidates_f = ["rival_f", "rival_m"]
    elif raw_key == "rivalfemale":
        candidates_m = ["rival_f", "rival_m"]
        candidates_f = ["rival_f", "rival_m"]
    elif raw_key == "player":
        candidates_m = ["player_m", "player_f"]
        candidates_f = ["player_f", "player_m"]

    for cm in candidates_m:
        if cm in trainer_keys_set:
            male_candidate = cm
            break
    for cf in candidates_f:
        if cf in trainer_keys_set:
            female_candidate = cf
            break
    if male_candidate or female_candidate:
        trainer_mappings.append({
            "trainerTypeId": tid,
            "maleKey": male_candidate or female_candidate,
            "femaleKey": female_candidate or male_candidate
        })

trainer_header = """// Generated pinned trainer sprite catalogue and metadata definitions.
#pragma once
#include <cstdint>
#include <cstring>

namespace Pokerogue3DS {

struct TrainerSpriteDefinition {
    const char* key;
    const char* texturePath;
    const char* metadataPath;
    uint16_t width;
    uint16_t height;
    uint16_t frameCount;
};

inline constexpr TrainerSpriteDefinition kTrainerSprites[] = {
"""
trainer_header += "\n".join(
    f'    {{"{r["key"]}", "{r["runtimePath"]}", '
    f'{"\"" + r["metadataPath"] + "\"" if r["metadataPath"] else "nullptr"}, '
    f'{r["width"]}, {r["height"]}, {r["frameCount"]}}},'
    for r in sorted(trainer_rows, key=lambda x: x["key"])
)
trainer_header += "\n};\n\n"

trainer_header += """struct TrainerTypeSpriteMap {
    uint16_t trainerTypeId;
    const char* maleKey;
    const char* femaleKey;
};

inline constexpr TrainerTypeSpriteMap kTrainerTypeSpriteMaps[] = {
"""
trainer_header += "\n".join(
    f'    {{{m["trainerTypeId"]}, "{m["maleKey"]}", "{m["femaleKey"]}"}},'
    for m in sorted(trainer_mappings, key=lambda x: x["trainerTypeId"])
)
trainer_header += """
};

inline const TrainerSpriteDefinition* findTrainerSpriteByKey(const char* key) {
    if (!key) return nullptr;
    for (const auto& def : kTrainerSprites) {
        if (std::strcmp(def.key, key) == 0) return &def;
    }
    return nullptr;
}

inline const TrainerSpriteDefinition* findTrainerSprite(uint16_t trainerTypeId, bool female = false) {
    for (const auto& map : kTrainerTypeSpriteMaps) {
        if (map.trainerTypeId == trainerTypeId) {
            const char* key = female ? map.femaleKey : map.maleKey;
            return findTrainerSpriteByKey(key);
        }
    }
    return nullptr;
}

inline const TrainerSpriteDefinition* findPlayerBackSprite(bool female = false) {
    const char* key = female ? "trainer_f_back" : "trainer_m_back";
    const auto* def = findTrainerSpriteByKey(key);
    if (def) return def;
    return findTrainerSpriteByKey("trainer_m_back");
}

} // namespace Pokerogue3DS
"""

(ROOT / "project/generated/include/content/TrainerSprites.hpp").write_text(trainer_header, encoding="utf-8", newline="\n")
(output.parent / "trainer-provenance.json").write_text(json.dumps({
    "repository": REPOSITORY, "revision": REVISION, "count": len(trainer_rows),
    "mappings": len(trainer_mappings), "files": trainer_rows
}, sort_keys=True, indent=2) + "\n", encoding="utf-8", newline="\n")
trainer_report = {
    "schemaVersion": 1, "repository": REPOSITORY, "revision": REVISION,
    "runtimeValidation": "NOT_EXECUTED",
    "animationSource": {"repository": "https://github.com/pagefaultgames/pokerogue",
        "revision": "8555c08c823b856cbec4eb99ca84ea52a955836d",
        "sourcePath": "src/data/trainers/trainer-config.ts", "frameRate": 24,
        "firstFrameNumber": 1, "lastFrameNumber": 128, "repeat": -1},
    "textureCount": len(trainer_rows),
    "metadataCount": sum(bool(r["metadataPath"]) for r in trainer_rows),
    "unrepresentedFrames": [{"key": r["key"], "sourcePath": r["sourcePath"],
        "sourceSHA256": r["sourceSHA256"],
        "reason": "NAME_NOT_REPRESENTABLE_IN_P3ATLAS1_AND_NOT_REFERENCED_BY_PINNED_TRAINER_ANIMATION",
        "records": r["extensions"]["unrepresentedFrames"]}
        for r in trainer_rows if r.get("extensions",{}).get("unrepresentedFrames")]
}
(ROOT / "docs/generated/TRAINER_PRESENTATION_REPORT.json").write_text(
    json.dumps(trainer_report,sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
print(f"Converted {len(trainer_rows)} trainer sprites and {len(trainer_mappings)} type mappings")

# ==============================================================================
# INTRO CINEMATIC PACKAGING (images/intro_dark.mp4)
# ==============================================================================
from prepare_intro_cinematic import prepare as prepare_intro
prepare_intro(ROOT)

from type_badges import prepare as prepare_type_labels
prepare_type_labels(ROOT)

from hud_type_icons import prepare as prepare_hud_types
prepare_hud_types(ROOT)

from starter_variant_icons import prepare as prepare_starter_variants
prepare_starter_variants(ROOT)
