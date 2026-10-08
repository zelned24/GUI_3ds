"""Static native-font coverage; does not compile or launch the game."""
import json,re,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from pixel_font import glyph_ink_bounds
class UiFontCoverage(unittest.TestCase):
    def test_non_ascii_ui_literals_have_ink_in_every_native_font(self):
        files=[ROOT/'project/generated/include/content/RuntimeUiText.hpp',ROOT/'project/src/main.cpp']
        for directory in ['project/include/runtime','project/src/runtime']:
            files.extend(p for p in (ROOT/directory).rglob('*') if p.suffix in ['.hpp','.cpp'])
        chars={'>'}
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
