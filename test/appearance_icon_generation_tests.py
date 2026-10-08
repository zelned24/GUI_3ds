"""Regenerate both native indices twice without compiling or converting textures."""
import hashlib
import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT / "scripts"))
from prepare_appearance_icons import write_index as write_physical_index
from resolve_appearance_icon_identities import write_index as write_identity_index
physical=json.loads((ROOT / "docs/generated/APPEARANCE_ICON_CONVERSION_REPORT.json").read_text(encoding="utf-8"))
identity=json.loads((ROOT / "docs/generated/APPEARANCE_ICON_IDENTITY_REPORT.json").read_text(encoding="utf-8"))
hashes=[]
for suffix in ("a","b"):
    destination=ROOT / "build" / ("appearance-index-replay-"+suffix)
    assert destination.resolve().parent==(ROOT / "build").resolve()
    (destination / "project/generated/include/content").mkdir(parents=True,exist_ok=True)
    write_physical_index(destination,physical)
    write_identity_index(destination,identity,physical)
    run={}
    for name in ("AppearanceIcons.hpp","AppearanceIconIdentities.hpp"):
        relative=Path("project/generated/include/content") / name
        generated=(destination / relative).read_bytes()
        assert generated==(ROOT / relative).read_bytes(),name+" differs from published generated file"
        run[name]=hashlib.sha256(generated).hexdigest()
    hashes.append(run)
assert hashes[0]==hashes[1]
print("PASS two-run byte-identical appearance indices:",json.dumps(hashes[0],sort_keys=True))
