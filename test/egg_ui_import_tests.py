"""Pinned egg text/threshold import checks; no native build."""
import hashlib,importlib.util,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("egg_ui",root/"scripts/prepare_egg_ui.py")
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
report=module.prepare(root)
for source in report["sources"]:
    repo=source["repository"].rsplit("/",1)[1]
    raw=subprocess.check_output(["git","-C",str(root/"build/upstream"/repo),"show",source["revision"]+":"+source["sourcePath"]])
    assert hashlib.sha256(raw).hexdigest()==source["sourceSHA256"]
    if repo=="pokerogue-locales": locale=json.loads(raw)
header=root/"project/generated/include/content/EggUiText.hpp"
report_path=root/"docs/generated/EGG_UI_IMPORT_REPORT.json"
assert hashlib.sha256(header.read_bytes()).hexdigest()==report["generatedSHA256"]
assert [int(value) for value,_ in report["thresholds"]]==[5,15,50]
for key,value in locale.items():
    assert json.dumps(key)+","+json.dumps(value,ensure_ascii=False) in header.read_text(encoding="utf-8")
assert 'if(key && !std::strcmp(row.key,key))' in header.read_text(encoding="utf-8")
before=(header.read_bytes(),report_path.read_bytes())
for _ in range(2):
    module.prepare(root)
    assert before==(header.read_bytes(),report_path.read_bytes())
print("PASS: pinned egg locales, 5/15/50 thresholds, provenance and deterministic generation")
