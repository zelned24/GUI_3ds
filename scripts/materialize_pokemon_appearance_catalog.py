"""Materialize every pinned shiny identity declared by the real asset tree/masterlist."""
import hashlib
import json
import re
import subprocess
from pathlib import Path
from pokemon_variant_palette import ROOT, materialize

KEY = r"[1-9][0-9]*(?:-[a-z0-9-]+)?"

def enumerate_appearances(paths, master):
    identities = set()
    for source in paths:
        match = re.fullmatch(r"images/pokemon/(back/)?(?:shiny/)?(female/)?(" + KEY + r")\.json", source)
        if match:
            identities.add((match[3], "back" if match[1] else "front", bool(match[2])))
    for facing in ("front", "back"):
        config = master.get("back", {}) if facing == "back" else master
        for female in (False, True):
            section = config.get("female", {}) if female else config
            for key, variants in section.items():
                if re.fullmatch(KEY, key):
                    if not isinstance(variants, list) or len(variants) != 3 or any(type(v) is not int or v not in (0, 1, 2) for v in variants):
                        raise ValueError("Invalid pinned variant declaration " + key)
                    identities.add((key, facing, female))
    jobs = []
    for key, facing, female in sorted(identities):
        config = master.get("back", {}) if facing == "back" else master
        if female:
            config = config.get("female", {})
        variants = config.get(key)
        for variant in (range(3) if variants is not None else (0,)):
            jobs.append((key, facing, female, variant))
    return jobs

def main():
    repo = ROOT / "build/upstream/pokerogue-assets"
    lock = json.loads((ROOT / "project/data/assets/pokerogue-sprite-lock.json").read_text())
    revision = lock["revision"]
    paths = subprocess.check_output(["git", "ls-tree", "-r", "--name-only", revision, "images/pokemon"], cwd=repo, text=True).splitlines()
    if subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=repo, text=True).strip() != revision:
        raise ValueError("Asset repository differs from pinned revision")
    master_bytes = subprocess.check_output(["git", "show", revision + ":images/pokemon/variant/_masterlist.json"], cwd=repo)
    master = json.loads(master_bytes)
    jobs = enumerate_appearances(paths, master)
    pinned_paths = set(paths)
    report = {"schemaVersion": 1, "repository": lock["repository"], "revision": revision,
        "masterlistSHA256": hashlib.sha256(master_bytes).hexdigest(),
        "catalogGeneratorSHA256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "requested": len(jobs), "materialized": [], "missingInUpstream": [], "unsupported": []}
    destination = ROOT / "build/upstream-assets/appearances"
    report_path = destination / "catalog-report.json"
    destination.mkdir(parents=True, exist_ok=True)
    for ordinal, (key, facing, female, variant) in enumerate(jobs):
        identity = {"atlasKey": key, "facing": facing, "female": female, "variant": variant}
        try:
            png, manifest, provenance = materialize(repo, key, facing, female, variant)
        except subprocess.CalledProcessError as error:
            # Only a missing git object path is upstream absence. Other git errors remain fatal.
            if error.cmd[:2] != ["git", "show"] or not error.cmd[2].startswith(revision + ":") or error.cmd[2].split(":", 1)[1] in pinned_paths:
                raise
            report["missingInUpstream"].append({**identity, "classification": "MISSING_IN_PINNED_UPSTREAM", "source": error.cmd[2]})
            continue
        except ValueError as error:
            report["unsupported"].append({**identity, "classification": "NOT_YET_SUPPORTED_BY_IMPORTER" if "Unsupported shader palette" in str(error) else "INVALID_IMPORT", "reason": str(error)})
            continue
        directory = destination / facing / ("female" if female else "default")
        directory.mkdir(parents=True, exist_ok=True)
        name = key + "-shiny-v" + str(variant)
        (directory / (name + ".png")).write_bytes(png)
        (directory / (name + ".json")).write_bytes(manifest)
        (directory / (name + "-provenance.json")).write_text(json.dumps(provenance, sort_keys=True, indent=2) + "\n", encoding="utf-8")
        report["materialized"].append({**identity, "pngSHA256": hashlib.sha256(png).hexdigest()})
        if ordinal % 50 == 0:
            report_path.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8")
            print(f"Materialized {len(report['materialized'])}/{len(jobs)} pinned appearances", flush=True)
    report["contentHash"] = hashlib.sha256(json.dumps(report, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    report_path.write_text(json.dumps(report, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"requested": len(jobs), "materialized": len(report["materialized"]), "missing": len(report["missingInUpstream"]), "unsupported": len(report["unsupported"])}))
    if report["unsupported"]:
        raise SystemExit("Some upstream appearances require explicit importer support")

if __name__ == "__main__":
    main()
