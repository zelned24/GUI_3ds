"""Run the existing ball import section without asset conversion or native build."""
import hashlib
import json
import re
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
output=ROOT/"build/native-presentation/source"
source=(ROOT/"scripts/prepare_native_presentation.py").read_text(encoding="utf-8")
section=source.split("# Reuse the existing upstream enum parser for native ball IDs.",1)[1].split("# Resolve only declarative literal iconImage arguments",1)[0]
icons=json.loads((output.parent/"item-icon-provenance.json").read_text(encoding="utf-8"))
strings=(ROOT/"project/generated/include/content/RuntimeUiText.hpp").read_text(encoding="utf-8")
ui_strings={json.loads(key):json.loads(value) for key,value in re.findall(r'\{("(?:[^"\\]|\\.)*"),("(?:[^"\\]|\\.)*")\}',strings)}
context=dict(ROOT=ROOT,output=output,subprocess=subprocess,hashlib=hashlib,json=json,re=re,item_frames=icons["frames"],ui_strings=ui_strings)
header=ROOT/"project/generated/include/content/BallMenuContent.hpp"
report_path=ROOT/"docs/generated/BALL_MENU_IMPORT_REPORT.json"
exec(section,context)
before=(header.read_bytes(),report_path.read_bytes())
exec(section,context)
assert before==(header.read_bytes(),report_path.read_bytes())
report=json.loads(report_path.read_text(encoding="utf-8"))
raw=subprocess.check_output(["git","-C",str(ROOT/"build/upstream/pokerogue"),"show",report["revision"]+":"+report["sourcePath"]])
assert hashlib.sha256(raw).hexdigest()==report["sourceSHA256"]
for repository,revision,path_key,hash_key in [
    ("pokerogue",report["revision"],"enumSourcePath","enumSourceSHA256"),
    ("pokerogue-locales",report["localeRevision"],"localeSourcePath","localeSourceSHA256")]:
    data=subprocess.check_output(["git","-C",str(ROOT/"build/upstream"/repository),"show",revision+":"+report[path_key]])
    assert hashlib.sha256(data).hexdigest()==report[hash_key]
body=raw.decode("utf-8").split("export function getPokeballCatchMultiplier",1)[1].split("export function",1)[0]
values=dict(re.findall(r"case PokeballType\.(\w+):\s*return (-?\d+(?:\.\d+)?);",body))
for row in report["entries"]:
    value=values[row["symbol"]]
    expected="100%" if float(value)<0 else value+"x"
    assert row["catchRateLabel"]==expected
    assert row["label"]==ui_strings[row["localeKey"]]
    assert json.dumps(expected).encode() in header.read_bytes()
    assert any(frame["key"]==row["iconKey"] for frame in icons["frames"])
ui=(ROOT/"project/include/runtime/BattleCommandMenuPresenter.hpp").read_text(encoding="utf-8")
assert "ball.catchRateLabel" in ui and "static const char* rates[]" not in ui
print("PASS pinned ball enum/rates, localized names, icon references and reproducible generation; native rendering pending")
