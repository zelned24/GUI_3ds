"""Baseline source contract checks; does not execute upstream TS or native C++."""
import json
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT / "scripts"))
from resolve_appearance_icon_identities import resolve
first=resolve(write=False)
assert first==resolve(write=False)
assert first==json.loads((ROOT / "docs/generated/APPEARANCE_ICON_IDENTITY_REPORT.json").read_text(encoding="utf-8"))
rows=first["records"]
identities={(row["dex"],row["formIndex"],row["female"],row["shiny"],row["variant"]):row for row in rows}
assert len(identities)==len(rows)
physical=json.loads((ROOT / "docs/generated/APPEARANCE_ICON_CONVERSION_REPORT.json").read_text(encoding="utf-8"))
paths={row["sourcePath"] for row in physical["files"]}
for row in rows:
    if row["status"]=="MISSING_IN_PINNED_ASSETS": assert row["resolvedSourcePath"] is None
    else: assert row["resolvedSourcePath"] in paths
    if row["status"]=="EXACT_PHYSICAL_ICON": assert row["requestedSourcePath"]==row["resolvedSourcePath"]
    assert row["extensions"]["eventReplacement"]=="NOT_YET_SUPPORTED"
assert identities[1,0,False,False,0]["requestedSourcePath"]=="images/pokemon/icons/1/1.png"
assert identities[1,0,False,True,0]["requestedSourcePath"]=="images/pokemon/icons/1/1s.png"
rare=identities[1,0,False,True,1]
assert rare["requestedSourcePath"]=="images/pokemon/icons/1/1_2.png"
assert rare["status"]=="UPSTREAM_CHECK_ICON_ID_NORMAL_FALLBACK" and rare["resolvedSourcePath"]=="images/pokemon/icons/1/1.png"
assert identities[255,0,True,True,0]["requestedSourcePath"]=="images/pokemon/icons/3/255s-f.png"
assert identities[1,0,True,False,0]["requestedSourcePath"]=="images/pokemon/icons/1/1.png"
print("PASS deterministic baseline identity mapping and explicit physical/fallback/missing classifications:",len(rows))
