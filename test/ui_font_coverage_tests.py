"""Static native-font coverage; does not compile or launch the game."""
import hashlib,io,json,re,struct,subprocess,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from pixel_font import glyph_ink_bounds, font_ink_bounds, monochrome_font
from PIL import ImageFont, __version__ as pillow_version, features
class UiFontCoverage(unittest.TestCase):
    def test_native_font_provenance_and_binary_alpha(self):
        report=json.loads((ROOT/'docs/generated/NATIVE_FONT_REPORT.json').read_text(encoding='utf-8'))
        self.assertEqual(report,json.loads((ROOT/'build/native-presentation/font-provenance.json').read_text(encoding='utf-8')))
        pinned=subprocess.check_output(['git','-C',str(ROOT/'build/upstream/pokerogue-assets'),'show',report['revision']+':'+report['sourcePath']])
        self.assertEqual(hashlib.sha256(pinned).hexdigest(),report['sourceSHA256'])
        for row in report['files']:
            data=(ROOT/'build/romfs'/row['convertedPath'].removeprefix('romfs:/')).read_bytes()
            self.assertEqual(hashlib.sha256(data).hexdigest(),row['convertedSHA256'])
            tglp=struct.unpack_from('<I',data,36)[0]
            sheet_size,sheets,texture_format=struct.unpack_from('<IHH',data,tglp+4)
            offset=struct.unpack_from('<I',data,tglp+20)[0]
            self.assertEqual(texture_format,11) # GPU_A4
            alpha=data[offset:offset+sheet_size*sheets]
            self.assertEqual(len(alpha),row['sheetBytes'])
            self.assertTrue(alpha)
            self.assertTrue(all(byte in (0,15,240,255) for byte in alpha))

    def test_every_native_glyph_matches_pinned_monochrome_source(self):
        report=json.loads((ROOT/'docs/generated/NATIVE_FONT_REPORT.json').read_text(encoding='utf-8'))
        self.assertEqual(report['rasterizer'],{'pillow':pillow_version,'freetype':features.version('freetype2')},
                         'Rasterizer changed: review/rebuild font assets explicitly')
        raw=subprocess.check_output(['git','-C',str(ROOT/'build/upstream/pokerogue-assets'),
                                     'show',report['revision']+':'+report['sourcePath']])
        for row in report['files']:
            font=ImageFont.truetype(io.BytesIO(raw),row['points']*96/72)
            def rasterize(cp):
                mask,offset=font.getmask2(chr(cp),mode='1',anchor='ls')
                return bytes(mask),mask.size,offset
            data=(ROOT/'build/romfs'/row['convertedPath'].removeprefix('romfs:/')).read_bytes()
            with self.subTest(points=row['points']):
                self.assertEqual(monochrome_font(data,rasterize),data,
                                 'Native glyph pixels differ from pinned source conversion')

    def test_layout_metrics_cover_accents_and_descenders(self):
        report=json.loads((ROOT/'docs/generated/NATIVE_FONT_REPORT.json').read_text(encoding='utf-8'))
        codepoints=[int(token,16) for token in (ROOT/'build/native-presentation/font-codepoints.txt').read_text().split()]
        tops=[];heights=[]
        for row in report['files']:
            data=(ROOT/'build/romfs'/row['convertedPath'].removeprefix('romfs:/')).read_bytes()
            top,bottom=font_ink_bounds(data,codepoints)
            self.assertEqual((row['textInkTop'],row['textInkHeight']),(top,bottom-top))
            tops.append(top);heights.append(bottom-top)
            for cp in codepoints:
                if chr(cp).isspace(): continue
                glyph_top,glyph_bottom=glyph_ink_bounds(data,cp)
                self.assertGreaterEqual(glyph_top,top)
                self.assertLessEqual(glyph_bottom,bottom)
            if row['points']==12:
                capital_top,capital_bottom=glyph_ink_bounds(data,ord('C'))
                self.assertLess(top,capital_top, 'Accents extend above capital C')
                self.assertGreater(bottom,capital_bottom, 'Descenders extend below capital C')
        header=(ROOT/'project/generated/include/content/NativeFontMetrics.hpp').read_text()
        self.assertIn('kNativeFontInkTop[]={'+','.join(map(str,tops))+'};',header)
        self.assertIn('kNativeFontInkHeight[]={'+','.join(map(str,heights))+'};',header)

    def test_ui_literals_have_ink_in_every_native_font(self):
        files=[ROOT/'project/generated/include/content/RuntimeUiText.hpp',ROOT/'project/generated/include/content/NatureUiNames.hpp',ROOT/'project/src/main.cpp']
        for directory in ['project/include/runtime','project/src/runtime']:
            files.extend(p for p in (ROOT/directory).rglob('*') if p.suffix in ['.hpp','.cpp'])
        chars={chr(i) for i in range(33,127)}
        for p in files:
            for literal in re.findall(r'"([^"\n]*)"',p.read_text(encoding='utf-8')):
                chars.update(c for c in literal if ord(c)>127 and not c.isspace())
        canonical=json.loads((ROOT/'project/data/pokerogue/canonical-content.json').read_text(encoding='utf-8'))['collections']
        for domain in ['species','forms','moves','abilities','items']:
            for entity in canonical[domain]:
                labels=[entity.get('name','')]+list(entity.get('names',{}).values())
                for label in labels:
                    chars.update(c for c in label if ord(c)>127 and not c.isspace())
        self.assertTrue(chars)
        fonts=sorted((ROOT/'build/romfs/presentation/fonts').glob('*.bcfnt'))
        self.assertEqual(len(fonts),4)
        for font in fonts:
            data=font.read_bytes()
            for char in sorted(chars):
                with self.subTest(font=font.name,codepoint=hex(ord(char))):
                    top,bottom=glyph_ink_bounds(data,ord(char))
                    self.assertGreater(bottom,top)
if __name__=='__main__': unittest.main()
