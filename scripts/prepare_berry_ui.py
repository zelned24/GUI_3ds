"""Bind canonical berry symbols to pinned locale and physical item icons."""
import hashlib,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
GAME_REV="8555c08c823b856cbec4eb99ca84ea52a955836d"
LOCALE_REV="23aea1cb0da5a0b15b836f3c243791591cc42303"
ASSET_REV="056a1f408f26a3be4fef243f7462cb43608c7928"
def prepare(root=ROOT):
    inputs=[("pokerogue",GAME_REV,"src/enums/berry-type.ts"),("pokerogue",GAME_REV,"src/data/berry.ts"),("pokerogue",GAME_REV,"src/modifier/modifier-type.ts"),("pokerogue-locales",LOCALE_REV,"es-ES/berry.json")]
    raw=[subprocess.check_output(["git","-C",str(root/"build/upstream"/repo),"show",rev+":"+path]) for repo,rev,path in inputs]
    symbols=re.findall(r'^  ([A-Z][A-Z_]*),',raw[0].decode(),re.M)
    locale=json.loads(raw[-1]);rows=[]
    icon_report=json.loads((root/"docs/generated/ITEM_ICON_TEXTURE_REPORT.json").read_text())
    if icon_report["revision"]!=ASSET_REV: raise ValueError("Berry asset index revision mismatch")
    icons=icon_report["files"]
    for symbol in symbols:
        key=symbol.lower();name=locale[key]["name"];icon=key+"_berry"
        reference=next((r for r in icons if r["key"]==icon),None)
        if not reference or not isinstance(name,str) or "{{" in name: raise ValueError("Unresolved berry presentation: "+symbol)
        rows.append(dict(symbol=symbol,localeKey="berry:"+key+".name",name=name,iconKey=icon,assetPath=reference["sourcePath"],assetSHA256=reference["sourceSHA256"]))
    header='// Generated pinned berry presentation.\n#pragma once\n#include <cstring>\nnamespace Pokerogue3DS {\nstruct BerryUiEntry {const char* symbol;const char* name;const char* iconKey;};\ninline constexpr BerryUiEntry kBerryUiEntries[]={\n'
    header+='\n'.join('    {'+json.dumps(r['symbol'])+','+json.dumps(r['name'],ensure_ascii=False)+','+json.dumps(r['iconKey'])+'},' for r in rows)
    header+='\n};\ninline const BerryUiEntry* berryUiEntry(const char* symbol) {if(!symbol) return nullptr;for(const auto& row:kBerryUiEntries) if(!std::strcmp(symbol,row.symbol)) return &row;return nullptr;}\n}\n'
    (root/"project/generated/include/content/BerryUiText.hpp").write_bytes(header.encode())
    report=dict(schemaVersion=1,sources=[dict(repository="https://github.com/pagefaultgames/"+repo,revision=rev,sourcePath=path,sourceSHA256=hashlib.sha256(data).hexdigest()) for (repo,rev,path),data in zip(inputs,raw)],assetRepository="https://github.com/pagefaultgames/pokerogue-assets",assetRevision=ASSET_REV,rows=rows)
    (root/"docs/generated/BERRY_UI_IMPORT_REPORT.json").write_bytes((json.dumps(report,sort_keys=True,indent=2,ensure_ascii=False)+"\n").encode())
    return report
if __name__=="__main__": print("Imported",len(prepare()["rows"]),"berry presentations")
