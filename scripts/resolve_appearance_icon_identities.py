"""Resolve pinned icon identities; preserve upstream fallback as explicit metadata."""
import hashlib
import json
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
GAME_REV="8555c08c823b856cbec4eb99ca84ea52a955836d"
ASSET_REV="056a1f408f26a3be4fef243f7462cb43608c7928"

def resolve(root=ROOT, write=True):
    raw=(root / "project/data/pokerogue/canonical-content.json").read_bytes()
    canonical=json.loads(raw)["collections"]
    icons=json.loads((root / "docs/generated/APPEARANCE_ICON_CONVERSION_REPORT.json").read_text(encoding="utf-8"))
    physical={row["sourcePath"]:row for row in icons["files"]}
    master_raw=subprocess.check_output(["git","-C",str(root / "build/upstream/pokerogue-assets"),"show",ASSET_REV+":images/pokemon/variant/_masterlist.json"])
    master=json.loads(master_raw)
    game_source=subprocess.check_output(["git","-C",str(root / "build/upstream/pokerogue"),"show",GAME_REV+":src/data/pokemon-species.ts"])
    fallback_source=subprocess.check_output(["git","-C",str(root / "build/upstream/pokerogue"),"show",GAME_REV+":src/ui/containers/pokedex-mon-container.ts"])
    # Exact gender branches inspected in pinned PokemonSpecies.getIconId.
    female_always={"doduo","dodrio","torchic","combusken","gible","gabite","hippopotas","hippowdon","unfezant","frillish","jellicent"}
    female_nonmega={"meganium","blaziken","garchomp","pyroar"}
    forms={}
    for form in canonical["forms"]: forms.setdefault(form["speciesId"],[]).append(form)
    rows=[]
    for species in sorted(canonical["species"],key=lambda row:row["speciesId"]):
        candidates=forms.get(species["id"],[]) or [{"extensions":{"upstreamFormIndex":0},"formSpriteKey":"","formKey":"BASE"}]
        for form in sorted(candidates,key=lambda row:row["extensions"]["upstreamFormIndex"]):
            sprite=form["formSpriteKey"]
            dex=species["speciesId"]
            variant_key=str(dex)+("-"+sprite if sprite else "")
            info=master.get(variant_key)
            if info is not None and (not isinstance(info,list) or len(info)!=3 or any(type(v) is not int or v not in (0,1,2) for v in info)):
                raise ValueError("Invalid variant metadata: "+variant_key)
            for female in (False,True):
                gender_suffix="-f" if female and (species["id"] in female_always or (species["id"] in female_nonmega and form["formKey"].lower().replace("_","-") not in ("mega","mega-z"))) else ""
                icon_form=sprite
                if species["id"]=="dudunsparce": icon_form=""
                elif species["id"] in ("zacian","zamazenta") and icon_form.startswith("behemoth"): icon_form="crowned"
                normal=str(dex)+gender_suffix+("-"+icon_form if icon_form else "")
                for shiny,variant in ((False,0),(True,0),(True,1),(True,2)):
                    special=bool(shiny and info and info[variant])
                    frame=str(dex)+("s" if shiny and not special else "")+gender_suffix+("-"+icon_form if icon_form else "")+("_"+str(variant+1) if special else "")
                    source=f"images/pokemon/icons/{species['generation']}/{frame}.png"
                    selected=source
                    status="EXACT_PHYSICAL_ICON"
                    if source not in physical:
                        selected=f"images/pokemon/icons/{species['generation']}/{normal}.png"
                        status="UPSTREAM_CHECK_ICON_ID_NORMAL_FALLBACK" if selected in physical else "MISSING_IN_PINNED_ASSETS"
                    rows.append({"dex":dex,"formIndex":form["extensions"]["upstreamFormIndex"],"female":female,"shiny":shiny,"variant":variant,
                        "requestedSourcePath":source,"resolvedSourcePath":selected if selected in physical else None,"status":status,
                        "extensions":{"variantDataKey":variant_key,"variantDeclaration":info,"eventReplacement":"NOT_YET_SUPPORTED"}})
    report={"schemaVersion":1,"gameRevision":GAME_REV,"assetRevision":ASSET_REV,
        "canonicalSHA256":hashlib.sha256(raw).hexdigest(),"variantMasterlistSHA256":hashlib.sha256(master_raw).hexdigest(),
        "sourceRepository":"https://github.com/pagefaultgames/pokerogue","sourcePath":"src/data/pokemon-species.ts",
        "sourceSHA256":hashlib.sha256(game_source).hexdigest(),"sourceSymbols":["PokemonSpecies.getIconId","PokemonSpecies.getVariantDataIndex"],
        "fallbackSourcePath":"src/ui/containers/pokedex-mon-container.ts","fallbackSourceSymbol":"PokedexMonContainer.checkIconId","fallbackSourceSHA256":hashlib.sha256(fallback_source).hexdigest(),
        "limitations":["Timed event replacements and experimental sprites are not yet supported; rows describe baseline pinned appearances."],"records":rows}
    target=root / "docs/generated/APPEARANCE_ICON_IDENTITY_REPORT.json"
    if write: target.write_bytes((json.dumps(report,sort_keys=True,indent=2)+"\n").encode("utf-8"))
    counts={}
    for row in rows: counts[row["status"]]=counts.get(row["status"],0)+1
    print(json.dumps(counts,sort_keys=True))
    return report
if __name__=="__main__": resolve()
