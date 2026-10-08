import io
import sys
import unittest
from pathlib import Path
from PIL import Image
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from pokemon_variant_palette import apply_palette, materialize, ROOT
from materialize_pokemon_appearance_catalog import enumerate_appearances

class VariantPaletteTests(unittest.TestCase):
    def test_exact_shader_rgb_alpha_and_no_chained_replacement(self):
        image = Image.new("RGBA", (4, 1))
        pixels = [(10, 20, 30, 255), (10, 20, 30, 7), (10, 20, 30, 0), (11, 20, 30, 255)]
        image.putdata(pixels)
        converted = apply_palette(image, {"0a141e": "112233", "112233": "ffffff"})
        self.assertEqual(list(converted.getdata()), [(17, 34, 51, 255), (17, 34, 51, 7), pixels[2], pixels[3]])
        self.assertEqual(list(image.getdata()), pixels)
    def test_invalid_and_unrepresentable_palettes_fail(self):
        image = Image.new("RGBA", (1, 1))
        for palette in ([], {"abcdef": None}, {4: "ffffff"},
                        {f"{i:06x}": "ffffff" for i in range(33)}):
            with self.assertRaises(ValueError): apply_palette(image, palette)
    def test_upstream_invalid_hex_is_black_and_first_match_wins(self):
        image = Image.new("RGBA", (3, 1))
        image.putdata([(156, 133, 93, 255), (0, 0, 0, 128), (171, 205, 239, 255)])
        palette = {"9c855d": "9e655cx", "broken": "112233", "000000": "ffffff",
                   "abcdef": "123456", "ABCDEF": "ffffff"}
        self.assertEqual(list(apply_palette(image, palette).getdata()),
                         [(0, 0, 0, 255), (17, 34, 51, 128), (18, 52, 86, 255)])

    def test_real_pinned_palette_and_dedicated_atlas_determinism(self):
        repo = ROOT / "build/upstream/pokerogue-assets"
        for key, facing, variant in (("1", "front", 1), ("1", "back", 2), ("2", "front", 1), ("626", "front", 1)):
            first = materialize(repo, key, facing, False, variant)
            self.assertEqual(first, materialize(repo, key, facing, False, variant))
            png, manifest, report = first
            self.assertEqual(list(Image.open(io.BytesIO(png)).size), report["dimensions"])
            self.assertTrue(manifest)
            self.assertGreaterEqual(len(report["sources"]), 6)
            self.assertTrue(all(len(source["sha256"]) == 64 for source in report["sources"]))
            self.assertEqual(report["runtimeStatus"], "SOURCE_MATERIALIZED_NOT_YET_T3X")
    def test_full_catalog_jobs_preserve_forms_gender_facing_and_variants(self):
        paths = ["images/pokemon/1.json", "images/pokemon/shiny/1.json", "images/pokemon/back/1.json",
                 "images/pokemon/female/25.json", "images/pokemon/6-mega-x.json", "images/pokemon/variant/1_2.json"]
        master = {"1": [1, 1, 1], "female": {"25": [0, 1, 2]}, "back": {"1": [1, 2, 1]}}
        jobs = enumerate_appearances(paths, master)
        self.assertEqual(len(jobs), 10)
        self.assertEqual(len(jobs), len(set(jobs)))
        self.assertIn(("6-mega-x", "front", False, 0), jobs)
        self.assertIn(("25", "front", True, 2), jobs)
        self.assertIn(("1", "back", False, 2), jobs)
        self.assertNotIn(("6-mega-x", "front", False, 1), jobs)
        self.assertEqual(jobs, enumerate_appearances(list(reversed(paths)), master))
        with self.assertRaises(ValueError): enumerate_appearances([], {"1": [1, 7, 1]})
    def test_invalid_identity_fails_before_source_lookup(self):
        for key, facing, variant in (("../1", "front", 1), ("1", "invalid", 1), ("1", "front", 3)):
            with self.assertRaises(ValueError): materialize(Path("absent"), key, facing, False, variant)

if __name__ == "__main__": unittest.main()
