import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from pixel_font import crisp_font
from prepare_azahar_preview import pixel_profile

class PixelFontTests(unittest.TestCase):
    def source(self):
        data = bytearray(84)
        struct.pack_into("<4sHHIII", data, 0, b"CFNT", 0xFEFF, 20, 0x3000000, len(data), 1)
        struct.pack_into("<4sI4B IHH HHHHI", data, 20, b"TGLP", 32, 8, 8, 7, 8, 32, 1, 11, 1, 1, 8, 8, 52)
        data[52:] = bytes((i * 17) % 256 for i in range(32))
        return bytes(data)
    def test_binary_alpha_and_metrics(self):
        source = self.source()
        converted = crisp_font(source)
        self.assertEqual(source[:52], converted[:52])
        self.assertEqual(len(source), len(converted))
        for before, after in zip(source[52:], converted[52:]):
            self.assertEqual(after >> 4, 15 if before >> 4 >= 8 else 0)
            self.assertEqual(after & 15, 15 if before & 15 >= 8 else 0)
        self.assertEqual(crisp_font(converted), converted)
    def test_preview_profile(self):
        source = "[Renderer]\ntexture_filter=4\n[System]\nis_new_3ds=true\n[WebService]\nsecret=do-not-copy\n"
        result = pixel_profile(source)
        self.assertIn("texture_filter=0", result)
        self.assertIn("texture_sampling=1", result)
        self.assertIn("filter_mode=false", result)
        self.assertIn("is_new_3ds=false", result)
        self.assertNotIn("secret", result)
        self.assertEqual(result, pixel_profile(source))

    def test_invalid_input(self):
        for position, value in [(4, 0), (12, 0), (24, 1), (38, 1), (48, 255)]:
            changed = bytearray(self.source()); changed[position] = value
            with self.assertRaises(ValueError): crisp_font(bytes(changed))
        with self.assertRaises(ValueError): crisp_font(b"not a font")
if __name__ == "__main__": unittest.main()
