"""Offline checks for original/derived padding source resolution; no native build."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("padding", Path(__file__).resolve().parents[1] / "scripts/pad_pokerogue_sprite_atlas.py")
padding = importlib.util.module_from_spec(spec)
spec.loader.exec_module(padding)


class PaddingSourceTests(unittest.TestCase):
    def test_original(self):
        root = Path("fixture")
        row = {"key": "269", "imagePath": "images/269.png", "manifestPath": "images/269.json", "imageSha256": "a" * 64}
        image, manifest, appearance, digest = padding.resolve_source(root, row, {})
        self.assertEqual(image, root / "build/upstream/pokerogue-assets/images/269.png")
        self.assertIsNone(appearance)
        self.assertEqual(digest, row["imageSha256"])

    def test_derived_identity_and_lineage(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            original = root / "build/upstream/pokerogue-assets/images/269.png"
            original.parent.mkdir(parents=True)
            original.write_bytes(b"fixture original")
            digest = hashlib.sha256(original.read_bytes()).hexdigest()
            report = {"repository": "fixture-repository", "revision": "fixture-pin"}
            for female in (False, True):
                directory = root / "build/upstream-assets/appearances/back" / ("female" if female else "default")
                directory.mkdir(parents=True)
                provenance = directory / "269-shiny-v2-provenance.json"
                data = {"schemaVersion": 1, "atlasKey": "269", "facing": "back", "female": female, "shiny": True, "variant": 2,
                        "pngSHA256": "b" * 64, "sources": [{**report, "sourcePath": "images/269.png", "sha256": digest}]}
                provenance.write_text(json.dumps(data), encoding="utf-8")
                row = {"key": "269" + ("-female" if female else "") + "-shiny-v2", "facing": "back", "imagePath": "images/269.png", "imageSha256": "b" * 64}
                image, manifest, appearance, actual = padding.resolve_source(root, row, report)
                self.assertEqual(image, directory / "269-shiny-v2.png")
                self.assertEqual(manifest, directory / "269-shiny-v2.json")
                self.assertEqual(actual, digest)
                self.assertEqual(appearance, data)
                data["variant"] = 1
                provenance.write_text(json.dumps(data), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "identity/hash"):
                    padding.resolve_source(root, row, report)
                data["variant"] = 2
                data["sources"][0]["sha256"] = "c" * 64
                provenance.write_text(json.dumps(data), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "lineage"):
                    padding.resolve_source(root, row, report)


if __name__ == "__main__":
    unittest.main()
