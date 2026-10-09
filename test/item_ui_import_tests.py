"""Validate resolved item names against pinned locale and source references."""
import hashlib,importlib.util,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("item_ui",root/"scripts/prepare_item_ui.py")
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
report=module.prepare(root)
raws=[]
for source in report["sources"]:
    repo=source["repository"].rsplit("/",1)[1]
    raw=subprocess.check_output(["git","-C",str(root/"build/upstream"/repo),"show",source["revision"]+":"+source["sourcePath"]])
    assert hashlib.sha256(raw).hexdigest()==source["sourceSHA256"]
    raws.append(raw)
locale=json.loads(raws[1])["ModifierType"]
assert report["rows"] and len({r["itemId"] for r in report["rows"]})==len(report["rows"])
for row in report["rows"]:
    key=row["key"].removeprefix("modifierType:ModifierType.").removesuffix(".name")
    expected=locale[key]["name"]
    for parameter,value in row.get("parameters",{}).items(): expected=expected.replace("{{"+parameter+"}}",value)
    assert row["name"]==expected and "{{" not in row["name"]
    if "parameters" in row:
        ball_locale=json.loads(raws[3])
        assert row["parameters"]["pokeballName"]==ball_locale[row["ballLocaleKey"].split(":",1)[1]]
        factory='PokeballType.'+row["ballSymbol"]+', '+row["parameters"]["modifierCount"]+')'
        assert factory in raws[0].decode("utf-8")
    assert 'modifierType:ModifierType.'+key in raws[0].decode("utf-8")
assert next(r["name"] for r in report["rows"] if r["itemId"]=="LURE")==locale["LURE"]["name"]
for item in ("POKEBALL","GREAT_BALL","ULTRA_BALL","ROGUE_BALL","MASTER_BALL"):
    assert any(row["itemId"]==item and "parameters" in row for row in report["rows"])
    assert not any(row["itemId"]==item for row in report["unsupported"])
assert any(r["itemId"]=="TM_COMMON" for r in report["unsupported"])
header=root/"project/generated/include/content/ItemUiNames.hpp"
report_path=root/"docs/generated/ITEM_UI_IMPORT_REPORT.json"
before=(header.read_bytes(),report_path.read_bytes())
for _ in range(2):
    module.prepare(root)
    assert before==(header.read_bytes(),report_path.read_bytes())
presenter=(root/"project/include/runtime/RewardMenuPresenter.hpp").read_text(encoding="utf-8")
assert 'itemUiName(reward->poolEntry->itemId)' in presenter
print(f"PASS: {len(report['rows'])} pinned resolved item names; dynamic variants explicitly unresolved")
