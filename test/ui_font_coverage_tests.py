"""Static native-font coverage; does not compile or launch the game."""
import hashlib,json,re,struct,subprocess,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from pixel_font import glyph_ink_bounds
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

    def test_ui_literals_have_ink_in_every_native_font(self):
        files=[ROOT/'project/generated/include/content/RuntimeUiText.hpp',ROOT/'project/src/main.cpp']
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
