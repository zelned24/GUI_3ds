"""Resolve pinned icon identities; preserve upstream fallback as explicit metadata."""
import hashlib
import json
import subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
GAME_REV="8555c08c823b856cbec4eb99ca84ea52a955836d"
ASSET_REV="056a1f408f26a3be4fef243f7462cb43608c7928"

def write_index(root, report, physical):
    indices={row["sourcePath"]:i for i,row in enumerate(physical["files"])}
    rows=[]
    for row in report["records"]:
        code=(8 if row["female"] else 0)|(4 if row["shiny"] else 0)|row["variant"]
        if not 0<row["dex"]<=65535 or not 0<=row["formIndex"]<=65535:
            raise ValueError("Icon identity exceeds canonical uint16 contract")
        index=indices.get(row["resolvedSourcePath"],65535)
        rows.append((row["dex"],row["formIndex"],code,index,int(row["status"]=="UPSTREAM_CHECK_ICON_ID_NORMAL_FALLBACK")))
    rows.sort(key=lambda row:row[:3])
    if len({row[:3] for row in rows})!=len(rows): raise ValueError("Duplicate icon identity")
    header="// Generated baseline identities; event replacements remain unsupported.\n#pragma once\n#include \"content/AppearanceIcons.hpp\"\nnamespace Pokerogue3DS {\n"
    header+="struct AppearanceIconIdentity {uint16_t dex,formIndex,physicalIndex;uint8_t appearance;bool upstreamFallback;};\ninline constexpr AppearanceIconIdentity kAppearanceIconIdentities[]={\n"
    for dex,form,code,index,fallback in rows:
        header+=f"    {{{dex},{form},{index},{code},{'true' if fallback else 'false'}}},\n"
    header+="};\n"+"""inline constexpr uint64_t appearanceIconIdentityKey(uint16_t dex,uint16_t form,uint8_t appearance) {
    return (uint64_t(dex)<<32)|(uint64_t(form)<<8)|appearance;
}
inline const AppearanceIconIdentity* findAppearanceIconIdentity(uint16_t dex,uint16_t form,bool female,bool shiny,uint8_t variant) {
    if(!dex || variant>2 || (!shiny && variant)) return nullptr;
    const uint8_t code=(female ? 8u : 0u)|(shiny ? 4u : 0u)|variant;
    const auto key=appearanceIconIdentityKey(dex,form,code);
    const std::size_t count=sizeof(kAppearanceIconIdentities)/sizeof(kAppearanceIconIdentities[0]);
    std::size_t first=0,last=count;
    while(first<last) {
        const auto middle=first+(last-first)/2;const auto& row=kAppearanceIconIdentities[middle];
        if(appearanceIconIdentityKey(row.dex,row.formIndex,row.appearance)<key) first=middle+1;
        else last=middle;
    }
    if(first==count) return nullptr;
    const auto& row=kAppearanceIconIdentities[first];
    return appearanceIconIdentityKey(row.dex,row.formIndex,row.appearance)==key ? &row : nullptr;
}
inline const AppearanceIconFrame* appearanceIconPhysicalFrame(const AppearanceIconIdentity* identity) {
    return identity && identity->physicalIndex<kAppearanceIconCount ? &kAppearanceIconFrames[identity->physicalIndex] : nullptr;
}
}
"""
    (root / "project/generated/include/content/AppearanceIconIdentities.hpp").write_bytes(header.encode("utf-8"))

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
    if write: write_index(root, report, icons)
    counts={}
    for row in rows: counts[row["status"]]=counts.get(row["status"],0)+1
    print(json.dumps(counts,sort_keys=True))
    return report
if __name__=="__main__": resolve()
