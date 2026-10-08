"""Validate generated berry names and assets against their pinned inputs."""
import hashlib,importlib.util,json,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("berry_ui",root/"scripts/prepare_berry_ui.py")
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
report=module.prepare(root)
for source in report["sources"]:
    repo=source["repository"].rsplit("/",1)[1]
    raw=subprocess.check_output(["git","-C",str(root/"build/upstream"/repo),"show",source["revision"]+":"+source["sourcePath"]])
    assert hashlib.sha256(raw).hexdigest()==source["sourceSHA256"]
    if repo=="pokerogue-locales": locale=json.loads(raw)
assert len(report["rows"])==11
for row in report["rows"]:
    assert row["name"]==locale[row["symbol"].lower()]["name"]
    png=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"show",report["assetRevision"]+":"+row["assetPath"]])
    assert hashlib.sha256(png).hexdigest()==row["assetSHA256"]
header=root/"project/generated/include/content/BerryUiText.hpp"
report_path=root/"docs/generated/BERRY_UI_IMPORT_REPORT.json"
before=(header.read_bytes(),report_path.read_bytes())
module.prepare(root)
assert before==(header.read_bytes(),report_path.read_bytes())
presenter=(root/"project/include/runtime/RewardMenuPresenter.hpp").read_text(encoding="utf-8")
assert 'type.id==static_cast<unsigned>(reward->berryType)' in presenter
assert 'm_icons.draw(renderer,berry->iconKey' in presenter
print("PASS: 11 berry names, real PNG hashes, deterministic generation and reward binding; GPU pending")
