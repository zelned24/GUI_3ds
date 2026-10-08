"""Generate nature labels from pinned enum/locales without compiling the runtime."""
import hashlib
import json
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
GAME_REV="8555c08c823b856cbec4eb99ca84ea52a955836d"
LOCALE_REV="23aea1cb0da5a0b15b836f3c243791591cc42303"
def prepare(root=ROOT):
    enum_path="src/enums/nature.ts"
    locale_path="es-ES/nature.json"
    enum_raw=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue"),"show",GAME_REV+":"+enum_path])
    locale_raw=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-locales"),"show",LOCALE_REV+":"+locale_path])
    script="import {PokerogueEnumParser} from './tools/js/data/PokerogueEnumParser.js'; const raw="+json.dumps(enum_raw.decode("utf-8"))+"; const parsed=new PokerogueEnumParser().parseEnum(raw,'Nature','src/enums/nature.ts'); console.log(JSON.stringify(Object.fromEntries(parsed.idToSymbol)));"
    symbols=json.loads(subprocess.check_output(["node","--input-type=module","-e",script],cwd=root))
    locales=json.loads(locale_raw)
    rows=[]
    for raw_id,symbol in sorted(symbols.items(),key=lambda entry:int(entry[0])):
        key=symbol.lower()
        if key not in locales or not isinstance(locales[key],str): raise ValueError("Missing pinned nature locale: "+symbol)
        rows.append({"id":int(raw_id),"symbol":symbol,"localeKey":"nature:"+key,"name":locales[key]})
    if [row["id"] for row in rows]!=list(range(25)): raise ValueError("Nature enum needs an explicit runtime compatibility update")
    header="// Generated from pinned Nature enum and Spanish locales.\n#pragma once\n#include <cstdint>\nnamespace Pokerogue3DS {\nstruct NatureUiName {uint8_t id;const char* name;};\ninline constexpr NatureUiName kNatureUiNames[]={\n"
    header+="\n".join("    {"+str(row["id"])+","+json.dumps(row["name"],ensure_ascii=False)+"}," for row in rows)
    header+="\n};\ninline const char* natureUiName(uint8_t id) {return id<sizeof(kNatureUiNames)/sizeof(kNatureUiNames[0]) ? kNatureUiNames[id].name : nullptr;}\n}\n"
    target=root/"project/generated/include/content/NatureUiNames.hpp";target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(header.encode("utf-8"))
    report={"schemaVersion":1,"sources":[{"repository":"https://github.com/pagefaultgames/pokerogue","revision":GAME_REV,"sourcePath":enum_path,"sourceSymbol":"Nature","sourceSHA256":hashlib.sha256(enum_raw).hexdigest()},{"repository":"https://github.com/pagefaultgames/pokerogue-locales","revision":LOCALE_REV,"sourcePath":locale_path,"sourceSymbol":"nature","sourceSHA256":hashlib.sha256(locale_raw).hexdigest()}],"rows":rows,"generatedSHA256":hashlib.sha256(target.read_bytes()).hexdigest()}
    report_path=root/"docs/generated/NATURE_UI_IMPORT_REPORT.json";report_path.parent.mkdir(parents=True,exist_ok=True);report_path.write_bytes((json.dumps(report,sort_keys=True,indent=2,ensure_ascii=False)+"\n").encode("utf-8"))
    return report
if __name__=="__main__":
    result=prepare();print("Generated",len(result["rows"]),"pinned nature names",result["generatedSHA256"])
