import hashlib,json,subprocess,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from starter_variant_icons import prepare,REVISION,GAME_REVISION
class StarterVariantTests(unittest.TestCase):
    def test_real_sources_frames_and_hashes(self):
        report=json.loads((ROOT/'build/native-presentation/starter-variant-provenance.json').read_text(encoding='utf-8'))
        self.assertEqual(report['revision'],REVISION)
        self.assertEqual(report['gameSource']['revision'],GAME_REVISION)
        self.assertEqual(report['physicalSize'],{'w':45,'h':14})
        self.assertEqual([key for key,_ in report['frames']],['0','1','2'])
        for repo,revision,sources in [('pokerogue-assets',REVISION,report['sources']),
            ('pokerogue',GAME_REVISION,[report['gameSource'],report['enumSource']])]:
            for source in sources:
                raw=subprocess.check_output(['git','-C',str(ROOT/'build/upstream'/repo),'show',revision+':'+source['sourcePath']])
                self.assertEqual(hashlib.sha256(raw).hexdigest(),source['sha256'])
        texture=ROOT/'build/romfs/presentation/ui/shiny_icons.t3x'
        self.assertEqual(hashlib.sha256(texture.read_bytes()).hexdigest(),report['convertedSHA256'])
    def test_generation_is_reproducible(self):
        paths=[ROOT/'project/generated/include/content/StarterVariantIcons.hpp',
            ROOT/'build/native-presentation/starter-variant-provenance.json',
            ROOT/'build/romfs/presentation/ui/shiny_icons.t3x']
        before=[p.read_bytes() for p in paths]
        prepare(ROOT)
        self.assertEqual(before,[p.read_bytes() for p in paths])
    def test_consumers_reject_invalid_indicator_geometry(self):
        report=json.loads((ROOT/'build/native-presentation/starter-variant-provenance.json').read_text(encoding='utf-8'))
        header=(ROOT/'project/generated/include/content/StarterVariantIcons.hpp').read_text(encoding='utf-8')
        size=report['physicalSize']
        self.assertIn(f"kStarterVariantIconWidth={size['w']},kStarterVariantIconHeight={size['h']}",header)
        for file in ['SetupPresenter.hpp','PartyMenuPresenter.hpp']:
            source=(ROOT/'project/include/runtime'/file).read_text(encoding='utf-8')
            self.assertIn('image.subtex->width!=kStarterVariantIconWidth',source)
            self.assertIn('image.subtex->height!=kStarterVariantIconHeight',source)
            self.assertIn('image.subtex->left>=image.subtex->right',source)
            self.assertIn('image.subtex->top<=image.subtex->bottom',source)
    def test_native_binding_uses_known_profile_and_nearest(self):
        source=(ROOT/'project/include/runtime/SetupPresenter.hpp').read_text(encoding='utf-8')
        self.assertIn('nativeStarterDefaultAppearance(*progress,shiny,variant) && shiny',source)
        self.assertIn('C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST)',source)
        self.assertIn('frame.width,frame.height,1.0f,kStarterVariantIconTints[variant]',source)
if __name__=='__main__': unittest.main()
