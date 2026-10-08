import struct
import sys
import unittest
import tempfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from pixel_font import crisp_font, compact_font, glyph_ink_bounds, monochrome_font
from prepare_azahar_preview import pixel_profile, prepare_portable_preview

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
    def compact_source(self):
        sheet=128;old_size=64*64//2;cwdh=sheet+old_size
        direct=cwdh+36;table=direct+24;scan=table+24
        data=bytearray(scan+32)
        struct.pack_into("<4sHHIII",data,0,b"CFNT",0xfeff,20,0x3000000,len(data),6)
        struct.pack_into("<4sI",data,20,b"FINF",32)
        data[29]=5;struct.pack_into("<III",data,36,60,cwdh+8,direct+8)
        data[48]=6
        struct.pack_into("<4sI4B IHH HHHHI",data,52,b"TGLP",32,3,3,2,3,old_size,1,11,16,16,64,64,sheet)
        data[sheet:sheet+256]=bytes([0xf0,0x0f,0xff,0])*64
        struct.pack_into("<4sIHHI",data,cwdh,b"CWDH",36,0,6,0)
        data[cwdh+16:cwdh+34]=bytes([0,3,4])*6
        struct.pack_into("<4sIHHHHIH",data,direct,b"CMAP",24,65,66,0,0,table+8,0)
        struct.pack_into("<4sIHHHHIHH",data,table,b"CMAP",24,67,68,1,0,scan+8,2,0xffff)
        struct.pack_into("<4sIHHHHIHHHHH",data,scan,b"CMAP",32,0xe9,0x2640,2,0,0,2,0xe9,4,0x2640,5)
        return bytes(data)

    def test_monochrome_preserves_strokes_metrics_and_maps(self):
        source=self.compact_source()
        raster=lambda cp: (bytes([0,255,0]),(1,3),(0,-2))
        result=monochrome_font(source,raster)
        self.assertEqual(result[:128],source[:128])
        cwdh=struct.unpack_from("<I",source,40)[0]
        for glyph in range(6):
            metric=cwdh+8+glyph*3
            self.assertEqual(result[metric],source[metric])
            self.assertEqual(result[metric+1],source[metric+1] if glyph==3 else 1)
            self.assertEqual(result[metric+2],source[metric+2])
        self.assertEqual(result[cwdh+28:],source[cwdh+28:])
        self.assertEqual(glyph_ink_bounds(result,ord('C')),(1,2))
        self.assertEqual(monochrome_font(result,raster),result)
        self.assertEqual(crisp_font(result),result)
        self.assertNotEqual(source,result)

    def test_monochrome_bitmap_width_does_not_clip_new_hinting(self):
        source=bytearray(self.compact_source())
        cwdh=struct.unpack_from("<I",source,40)[0]
        source[cwdh+8+2*3+1]=1 # C's old grayscale bitmap width.
        result=monochrome_font(bytes(source),lambda cp:(bytes([255,255]),(2,1),(0,-1)))
        self.assertEqual(result[cwdh+8+2*3+1],2)
        self.assertEqual(result[cwdh+8+2*3+2],source[cwdh+8+2*3+2]) # Advance retained.
        self.assertEqual(glyph_ink_bounds(result,ord('C')),(1,2))

    def test_monochrome_replaces_stale_bitmap_width(self):
        source=bytearray(self.compact_source())
        cwdh=struct.unpack_from("<I",source,40)[0]
        metric=cwdh+8+2*3
        source[metric+1]=30
        result=monochrome_font(bytes(source),lambda cp:(bytes([255,255]),(2,1),(0,-1)))
        self.assertEqual(result[metric+1],2)
        self.assertEqual(result[metric],source[metric])
        self.assertEqual(result[metric+2],source[metric+2])

    def test_monochrome_rejects_clipped_ink_and_invalid_masks(self):
        source=self.compact_source()
        with self.assertRaisesRegex(ValueError,"exceeds native cell"):
            monochrome_font(source,lambda cp:(bytes([255]),(1,1),(0,2)))
        with self.assertRaisesRegex(ValueError,"mask"):
            monochrome_font(source,lambda cp:(bytes([]),(1,1),(0,0)))

    def test_compact_preserves_glyphs_metrics_and_links(self):
        source=self.compact_source();result=compact_font(source)
        delta=64*64//2-64*8//2
        self.assertEqual(len(result),len(source)-delta)
        self.assertEqual(struct.unpack_from("<I",result,12)[0],len(result))
        self.assertEqual(struct.unpack_from("<H",result,78)[0],8)
        self.assertEqual(struct.unpack_from("<H",result,74)[0],2)
        self.assertEqual(source[128:384],result[128:384])
        self.assertEqual(source[28:36],result[28:36])
        self.assertEqual(source[48:64],result[48:64])
        cwdh,direct=struct.unpack_from("<II",result,40)
        self.assertEqual(source[128+2048+16:128+2048+34],result[cwdh+8:cwdh+26])
        expected=[(0,[0]),(1,[2,0xffff]),(2,[2,0xe9,4,0x2640,5])]
        for method,payload in expected:
            self.assertEqual(result[direct-8:direct-4],b"CMAP")
            self.assertEqual(struct.unpack_from("<H",result,direct+4)[0],method)
            self.assertEqual(list(struct.unpack_from("<"+"H"*len(payload),result,direct+12)),payload)
            direct=struct.unpack_from("<I",result,direct+8)[0]
        self.assertEqual(direct,0)
        self.assertEqual(compact_font(result),result)
        self.assertEqual(crisp_font(result),result)

    def test_multi_sheet_preserves_original_sheet_indices(self):
        original=self.compact_source();old_end=128+2048
        data=bytearray(original[:old_end]+bytes(2048)+original[old_end:])
        struct.pack_into("<I",data,12,len(data));struct.pack_into("<H",data,68,2)
        for field in (40,44):
            struct.pack_into("<I",data,field,struct.unpack_from("<I",original,field)[0]+2048)
        pointer=struct.unpack_from("<I",original,44)[0]
        while pointer:
            next_pointer=struct.unpack_from("<I",original,pointer+8)[0]
            if next_pointer: struct.pack_into("<I",data,pointer+2048+8,next_pointer+2048)
            pointer=next_pointer
        self.assertEqual(compact_font(bytes(data)),bytes(data))

    def test_unknown_sections_and_cycles_fail_explicitly(self):
        source=self.compact_source();data=bytearray(source+b"XXXX"+struct.pack("<I",8))
        struct.pack_into("<I",data,12,len(data));struct.pack_into("<I",data,16,7)
        with self.assertRaisesRegex(ValueError,"Unsupported"): compact_font(bytes(data))
        data=bytearray(source);pointer=struct.unpack_from("<I",data,44)[0]
        struct.pack_into("<I",data,pointer+8,pointer)
        with self.assertRaisesRegex(ValueError,"Cyclic"): compact_font(bytes(data))

    def test_compact_rejects_loss_or_invalid_offsets(self):
        source=self.compact_source()
        self.assertEqual(glyph_ink_bounds(source,65),(1,2))
        for position,value in [(384,15),(40,0xff),(76,63),(74,15)]:
            changed=bytearray(source);changed[position]=value
            with self.assertRaises(ValueError): compact_font(bytes(changed))
        changed=bytearray(source);changed[:4]=b"CFNU"
        with self.assertRaises(ValueError): compact_font(bytes(changed))

    def test_native_ink_bounds_and_invalid_reference(self):
        source=self.compact_source()
        self.assertEqual(glyph_ink_bounds(source,65),glyph_ink_bounds(compact_font(source),65))
        top,bottom=glyph_ink_bounds(source,65)
        self.assertGreaterEqual(top,0)
        self.assertLessEqual(bottom,3)
        self.assertGreater(bottom,top)
        with self.assertRaisesRegex(ValueError,"Missing reference"): glyph_ink_bounds(source,0x1234)
        with self.assertRaises(ValueError): glyph_ink_bounds(b"invalid",65)
        blank=bytearray(source)
        blank[128:128+64*64//2]=bytes(64*64//2)
        with self.assertRaisesRegex(ValueError,"Empty reference"): glyph_ink_bounds(bytes(blank),65)

    def test_preview_profile(self):
        source = "[Renderer]\ntexture_filter=4\n[System]\nis_new_3ds=true\n[WebService]\nsecret=do-not-copy\n"
        result = pixel_profile(source)
        self.assertIn("texture_filter=0", result)
        self.assertIn("texture_sampling=1", result)
        self.assertIn("filter_mode=false", result)
        self.assertIn("is_new_3ds=false", result)
        self.assertNotIn("secret", result)
        self.assertEqual(result, pixel_profile(source))

    def test_portable_preview_uses_adjacent_profile_and_preserves_installation(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);installed=root/'installed';installed.mkdir()
            exe=installed/'azahar.exe';exe.write_bytes(b'executable')
            (installed/'Qt6Core.dll').write_bytes(b'library')
            (installed/'private.ini').write_text('do-not-copy')
            (installed/'plugins').mkdir();(installed/'plugins/platform.dll').write_bytes(b'plugin')
            profile=pixel_profile('[Renderer]\ntexture_filter=4\n')
            target=root/'preview'
            result=prepare_portable_preview(exe,target,profile)
            self.assertEqual(result.read_bytes(),exe.read_bytes())
            self.assertEqual((target/'user/config/qt-config.ini').read_text(),profile)
            self.assertEqual((target/'plugins/platform.dll').read_bytes(),b'plugin')
            self.assertFalse((target/'private.ini').exists())
            (target/'user/sdmc').mkdir();(target/'user/sdmc/save.dat').write_bytes(b'save')
            prepare_portable_preview(exe,target,profile)
            self.assertEqual((target/'user/sdmc/save.dat').read_bytes(),b'save')
            self.assertEqual((installed/'private.ini').read_text(),'do-not-copy')
            for invalid in (installed,installed/'preview',root):
                with self.assertRaises(ValueError): prepare_portable_preview(exe,invalid,profile)
            with self.assertRaises(ValueError): prepare_portable_preview(installed/'absent.exe',target,profile)

    def test_invalid_input(self):
        for position, value in [(4, 0), (12, 0), (24, 1), (38, 1), (48, 255)]:
            changed = bytearray(self.source()); changed[position] = value
            with self.assertRaises(ValueError): crisp_font(bytes(changed))
        with self.assertRaises(ValueError): crisp_font(b"not a font")
if __name__ == "__main__": unittest.main()
