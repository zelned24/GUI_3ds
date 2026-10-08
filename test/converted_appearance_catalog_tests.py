"""Real generated catalog integration checks. Requires completed local assets.
This checks inventory coverage/provenance and generated index, not native loading.
"""
import hashlib
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]

class ConvertedAppearanceCatalogTests(unittest.TestCase):
    def test_full_materialized_coverage(self):
        catalog=json.loads((ROOT/"build/upstream-assets/appearances/catalog-report.json").read_text(encoding="utf-8"))
        inventory=json.loads((ROOT/"build/upstream-assets/converted-sprite-assets.json").read_text(encoding="utf-8"))
        expected={(row["atlasKey"],row["facing"],row["female"],row["variant"]):row["pngSHA256"] for row in catalog["materialized"]}
        actual={}
        for row in inventory["assets"]:
            appearance=row.get("appearance")
            if not appearance or not appearance["shiny"]: continue
            key=(appearance["atlasKey"],appearance["facing"],appearance["female"],appearance["variant"])
            self.assertNotIn(key,actual)
            actual[key]=appearance["pngSHA256"]
        self.assertGreater(len(expected),0)
        self.assertEqual(len(expected),len(catalog["materialized"]))
        self.assertEqual(actual,expected)

    def test_report_and_generated_header(self):
        raw=(ROOT/"build/upstream-assets/converted-sprite-assets.json").read_bytes()
        inventory=json.loads(raw)
        report=json.loads((ROOT/"docs/generated/POKEMON_APPEARANCE_CONVERSION_REPORT.json").read_text(encoding="utf-8"))
        self.assertEqual(report["inventorySHA256"],hashlib.sha256(raw).hexdigest())
        for key in ("repository","revision","atlasCount","textureCount"):
            self.assertEqual(report[key],inventory[key])
        self.assertEqual(report["atlasCount"],len(inventory["assets"]))
        self.assertEqual(report["textureCount"],sum(len(row["textures"]) for row in inventory["assets"]))
        self.assertEqual(report["textureBytesOnDisk"],inventory["textureBytes"])
        appearances=[row["appearance"] for row in inventory["assets"] if row.get("appearance")]
        self.assertEqual(report["appearanceCount"],len(appearances))
        self.assertEqual(report["shinyAppearanceCount"],sum(row["shiny"] for row in appearances))
        self.assertEqual(report["normalFemaleAppearanceCount"],sum(not row["shiny"] and row["female"] for row in appearances))
        header=(ROOT/"project/generated/include/content/PokemonAppearanceAssets.hpp").read_text(encoding="utf-8")
        self.assertIn(report["inventorySHA256"],header)
        self.assertIn(f"std::array<PokemonAppearanceAsset, {len(appearances)}>",header)
        self.assertEqual(report["runtimeValidation"],"NOT_EXECUTED")

if __name__=="__main__": unittest.main()
