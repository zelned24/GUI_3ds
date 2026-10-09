"""Import pinned item names and statically resolved ball/voucher-package parameters."""
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
    ball_source=read("pokerogue",GAME_REV,"src/data/pokeball.ts")
    ball_locale=read("pokerogue-locales",LOCALE_REV,"es-ES/pokeball.json")
    ball_names=json.loads(ball_locale)
    name_function=ball_source.decode("utf-8").split("export function getPokeballName",1)[1].split("export function",1)[0]
    ball_keys=dict(re.findall(r'case PokeballType\.(\w+):\s*ret = i18next.t\("([^"\\]+)"',name_function))
    voucher_source=read("pokerogue",GAME_REV,"src/system/voucher.ts")
    voucher_locale=read("pokerogue-locales",LOCALE_REV,"es-ES/voucher.json")
    voucher_names=json.loads(voucher_locale)
    voucher_function=voucher_source.decode("utf-8").split("export function getVoucherTypeName",1)[1].split("export function",1)[0]
    voucher_keys=dict(re.findall(r'case VoucherType\.(\w+):\s*return i18next.t\("([^"\\]+)"',voucher_function))
    source=game.decode("utf-8").split("const modifierTypeInitObj = Object.freeze({",1)[1].split("\n});",1)[0]
    entries=list(re.finditer(r'^  ([A-Z][A-Z0-9_]*):',source,re.M))
    rows=[];unsupported=[]
    for i,match in enumerate(entries):
        item=match.group(1);body=source[match.end():entries[i+1].start() if i+1<len(entries) else len(source)]
        ball=re.search(r'new AddPokeballModifierType\("[^"\\]+",\s*PokeballType\.(\w+),\s*(\d+)\)',body)
        if ball:
            symbol,count=ball.groups()
            locale_key=ball_keys.get(symbol)
            if not locale_key or not locale_key.startswith("pokeball:"):
                raise ValueError("Unresolved ball name: "+item)
            ball_name=ball_names.get(locale_key.split(":",1)[1])
            template=names["AddPokeballModifierType"]["name"]
            if not isinstance(ball_name,str) or not isinstance(template,str):
                raise ValueError("Invalid ball localization: "+item)
            parameters={"modifierCount":count,"pokeballName":ball_name}
            name=template
            for parameter,value in parameters.items(): name=name.replace("{{"+parameter+"}}",value)
            if "{{" in name: raise ValueError("Unresolved ball name parameter: "+item)
            rows.append(dict(itemId=item,key="modifierType:ModifierType.AddPokeballModifierType.name",name=name,
                sourceSymbol="modifierTypeInitObj."+item,resolverSourceSymbol="AddPokeballModifierType.name/getPokeballName",
                ballSymbol=symbol,ballLocaleKey=locale_key,parameters=parameters))
            continue
        voucher=re.search(r'new AddVoucherModifierType\(VoucherType\.(\w+),\s*(\d+)\)',body)
        if voucher:
            symbol,count=voucher.groups()
            locale_key=voucher_keys.get(symbol)
            if not locale_key or not locale_key.startswith("voucher:"):
                raise ValueError("Unresolved voucher name: "+item)
            voucher_name=voucher_names.get(locale_key.split(":",1)[1])
            template=names["AddVoucherModifierType"]["name"]
            if not isinstance(voucher_name,str) or not isinstance(template,str):
                raise ValueError("Invalid voucher localization: "+item)
            parameters={"modifierCount":count,"voucherTypeName":voucher_name}
            name=template
            for parameter,value in parameters.items(): name=name.replace("{{"+parameter+"}}",value)
            if "{{" in name: raise ValueError("Unresolved voucher parameter: "+item)
            rows.append(dict(itemId=item,key="modifierType:ModifierType.AddVoucherModifierType.name",name=name,
                sourceSymbol="modifierTypeInitObj."+item,resolverSourceSymbol="AddVoucherModifierType.name/getVoucherTypeName",
                voucherSymbol=symbol,voucherLocaleKey=locale_key,parameters=parameters))
            continue
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
    header="// Generated pinned item presentation names.\n#pragma once\n#include <cstring>\nnamespace Pokerogue3DS {\nstruct ItemUiName {const char* itemId;const char* name;};\ninline constexpr ItemUiName kItemUiNames[]={\n"
    header+="\n".join("    {"+json.dumps(r["itemId"])+","+json.dumps(r["name"],ensure_ascii=False)+"}," for r in rows)
    header+='\n};\ninline const char* itemUiName(const char* id) {if(!id) return nullptr;for(const auto& row:kItemUiNames) if(!std::strcmp(row.itemId,id)) return row.name;return nullptr;}\n}\n'
    (root/"project/generated/include/content/ItemUiNames.hpp").write_bytes(header.encode())
    report=dict(schemaVersion=1,sources=[dict(repository="https://github.com/pagefaultgames/pokerogue",revision=GAME_REV,sourcePath="src/modifier/modifier-type.ts",sourceSHA256=hashlib.sha256(game).hexdigest()),dict(repository="https://github.com/pagefaultgames/pokerogue-locales",revision=LOCALE_REV,sourcePath="es-ES/modifier-type.json",sourceSHA256=hashlib.sha256(locale).hexdigest())],rows=rows,unsupported=unsupported)
    report["sources"].extend([
        dict(repository="https://github.com/pagefaultgames/pokerogue",revision=GAME_REV,sourcePath="src/data/pokeball.ts",sourceSHA256=hashlib.sha256(ball_source).hexdigest()),
        dict(repository="https://github.com/pagefaultgames/pokerogue-locales",revision=LOCALE_REV,sourcePath="es-ES/pokeball.json",sourceSHA256=hashlib.sha256(ball_locale).hexdigest())])
    report["sources"].extend([
        dict(repository="https://github.com/pagefaultgames/pokerogue",revision=GAME_REV,sourcePath="src/system/voucher.ts",sourceSHA256=hashlib.sha256(voucher_source).hexdigest()),
        dict(repository="https://github.com/pagefaultgames/pokerogue-locales",revision=LOCALE_REV,sourcePath="es-ES/voucher.json",sourceSHA256=hashlib.sha256(voucher_locale).hexdigest())])
    (root/"docs/generated/ITEM_UI_IMPORT_REPORT.json").write_bytes((json.dumps(report,sort_keys=True,indent=2,ensure_ascii=False)+"\n").encode())
    return report
if __name__=="__main__": print("Imported",len(prepare()["rows"]),"resolved item names")
