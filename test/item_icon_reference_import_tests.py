"""Exercise production icon reference import against pinned physical sources."""
import hashlib,json,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/"scripts/prepare_native_presentation.py").read_text(encoding="utf-8")
section=source.split("# Resolve only declarative literal iconImage arguments from inspected constructors.\n",1)[1].split("entity_header=",1)[0]
frames=json.loads((ROOT/"build/native-presentation/item-icon-provenance.json").read_text(encoding="utf-8"))["frames"]
context=dict(ROOT=ROOT,output=ROOT/"build/native-presentation/source",subprocess=subprocess,hashlib=hashlib,json=json,re=re,source_revision="8555c08c823b856cbec4eb99ca84ea52a955836d",canonical=json.loads((ROOT/"project/data/pokerogue/canonical-content.json").read_text(encoding="utf-8"))["collections"],item_frames=frames)
header=ROOT/"project/generated/include/content/ItemIconReferences.hpp"
report=ROOT/"docs/generated/ITEM_ICON_REFERENCE_REPORT.json"
exec(section,context)
first=(header.read_bytes(),report.read_bytes())
exec(section,context)
assert first==(header.read_bytes(),report.read_bytes())
data=json.loads(report.read_text(encoding="utf-8"))
resolved={row["id"]:row for row in data["resolved"]}
for item,key in {"DIRE_HIT":"dire_hit","VOUCHER":"coupon","VOUCHER_PLUS":"pair_of_tickets","VOUCHER_PREMIUM":"mystic_ticket"}.items():
    assert resolved[item]["iconKey"]==key
for row in data["resolved"]:
    assert any(frame["key"]==row["iconKey"] for frame in frames)
    if "resolverSourcePath" in row:
        raw=subprocess.check_output(["git","-C",str(ROOT/"build/upstream/pokerogue"),"show",data["revision"]+":"+row["resolverSourcePath"]])
        assert hashlib.sha256(raw).hexdigest()==row["resolverSourceSHA256"]
        assert row["resolverSourceSymbol"]=="getVoucherTypeIcon"
assert "TM_COMMON" not in resolved
assert any(row["id"]=="TM_COMMON" for row in data["unsupported"])
print("PASS real icon reference resolution, variant provenance, physical keys and deterministic generation; GPU pending")
