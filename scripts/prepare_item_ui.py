"""Import literal item locale names referenced by pinned modifier factories."""
import hashlib,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
GAME_REV="8555c08c823b856cbec4eb99ca84ea52a955836d"
LOCALE_REV="23aea1cb0da5a0b15b836f3c243791591cc42303"
def prepare(root=ROOT):
    def read(repo,rev,path): return subprocess.check_output(["git","-C",str(root/"build/upstream"/repo),"show",rev+":"+path])
    game=read("pokerogue",GAME_REV,"src/modifier/modifier-type.ts")
    locale=read("pokerogue-locales",LOCALE_REV,"es-ES/modifier-type.json")
    names=json.loads(locale)["ModifierType"]
    source=game.decode("utf-8").split("const modifierTypeInitObj = Object.freeze({",1)[1].split("\n});",1)[0]
    entries=list(re.finditer(r'^  ([A-Z][A-Z0-9_]*):',source,re.M))
    rows=[];unsupported=[]
    for i,match in enumerate(entries):
        item=match.group(1);body=source[match.end():entries[i+1].start() if i+1<len(entries) else len(source)]
        keys=re.findall(r'["\']modifierType:ModifierType\.([^"\']+)["\']',body)
        resolved=[]
        for key in keys:
            value=names.get(key)
            name=value.get("name") if isinstance(value,dict) else None
            if isinstance(name,str) and "{{" not in name: resolved.append((key,name))
        if len(resolved)==1:
            key,name=resolved[0]
            rows.append(dict(itemId=item,key="modifierType:ModifierType."+key+".name",name=name,sourceSymbol="modifierTypeInitObj."+item))
        else: unsupported.append(dict(itemId=item,reason="PARAMETERIZED_OR_NON_LITERAL_NAME"))
    header="// Generated literal pinned item presentation names.\n#pragma once\n#include <cstring>\nnamespace Pokerogue3DS {\nstruct ItemUiName {const char* itemId;const char* name;};\ninline constexpr ItemUiName kItemUiNames[]={\n"
    header+="\n".join("    {"+json.dumps(r["itemId"])+","+json.dumps(r["name"],ensure_ascii=False)+"}," for r in rows)
    header+='\n};\ninline const char* itemUiName(const char* id) {if(!id) return nullptr;for(const auto& row:kItemUiNames) if(!std::strcmp(row.itemId,id)) return row.name;return nullptr;}\n}\n'
    (root/"project/generated/include/content/ItemUiNames.hpp").write_bytes(header.encode())
    report=dict(schemaVersion=1,sources=[dict(repository="https://github.com/pagefaultgames/pokerogue",revision=GAME_REV,sourcePath="src/modifier/modifier-type.ts",sourceSHA256=hashlib.sha256(game).hexdigest()),dict(repository="https://github.com/pagefaultgames/pokerogue-locales",revision=LOCALE_REV,sourcePath="es-ES/modifier-type.json",sourceSHA256=hashlib.sha256(locale).hexdigest())],rows=rows,unsupported=unsupported)
    (root/"docs/generated/ITEM_UI_IMPORT_REPORT.json").write_bytes((json.dumps(report,sort_keys=True,indent=2,ensure_ascii=False)+"\n").encode())
    return report
if __name__=="__main__": print("Imported",len(prepare()["rows"]),"literal item names")
