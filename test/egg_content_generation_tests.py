import importlib.util
import hashlib
import json
import re
import subprocess
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
import sys
sys.path.insert(0,str(ROOT/'scripts'))
from prepare_egg_content import prepare, GAME_REV
class EggContentTests(unittest.TestCase):
    def test_provenance_and_values(self):
        report=prepare()
        for source in report['sources']:
            raw=subprocess.check_output(['git','-C',str(ROOT/'build/upstream/pokerogue'),'show',GAME_REV+':'+source['sourcePath']])
            self.assertEqual(hashlib.sha256(raw).hexdigest(),source['hash'])
        self.assertEqual(report['incubationConstants'],{'HATCH_WAVES_COMMON_EGG':10,'HATCH_WAVES_RARE_EGG':25,'HATCH_WAVES_EPIC_EGG':50,'HATCH_WAVES_LEGENDARY_EGG':100,'HATCH_WAVES_MANAPHY_EGG':50})
        self.assertEqual(report['runtimeIntegration'],'PENDING_INVENTORY_GACHA_HATCHING')
        self.assertEqual(report['gachaInventoryLimit'],99)
        self.assertEqual([(r['tier'],r['minimum'],r['maximum']) for r in report['speciesCostBounds']],[('COMMON',1,3),('RARE',4,5),('EPIC',6,7),('LEGENDARY',8,9)])
        self.assertEqual([(r['symbol'],r['id']) for r in report['excludedSpecies']],[('PHIONE',489),('MANAPHY',490),('ETERNATUS',890)])
        self.assertEqual([(o['voucher'],o['consumed'],o['pulls']) for o in report['voucherOffers']],[('REGULAR',1,1),('REGULAR',10,10),('PLUS',1,5),('PREMIUM',1,10),('GOLDEN',1,25)])
        self.assertEqual(report['pityThresholds'],{'EGG_PITY_RARE_THRESHOLD':9,'EGG_PITY_EPIC_THRESHOLD':59,'EGG_PITY_LEGENDARY_THRESHOLD':412})
        self.assertEqual(report['gachaThresholds'],{'GACHA_DEFAULT_COMMON_EGG_THRESHOLD':52,'GACHA_DEFAULT_RARE_EGG_THRESHOLD':8,'GACHA_DEFAULT_EPIC_EGG_THRESHOLD':1,'GACHA_LEGENDARY_UP_THRESHOLD_OFFSET':1})
        self.assertEqual(report['specialIncubationSpecies'],[{'symbol':'PHIONE','id':489},{'symbol':'MANAPHY','id':490}])
        canonical=(ROOT/report['canonicalInput']['sourcePath']).read_bytes()
        self.assertEqual(hashlib.sha256(canonical).hexdigest(),report['canonicalInput']['hash'])
        tiers={row['dex']:row['tier'] for row in report['speciesTiers']}
        self.assertEqual(tiers[150],'LEGENDARY')
        self.assertEqual(tiers[1],'COMMON')
        self.assertEqual(len(set(tiers.values())),4)
        self.assertTrue(next(row for row in report['speciesTiers'] if row['dex']==1)['declared'])
        self.assertFalse(next(row for row in report['speciesTiers'] if row['dex']==2)['declared'])
        by_dex={s['nationalDexId']:s for s in json.loads(canonical)['collections']['species']}
        for row in report['speciesTiers']:
            declaration=re.search(r'\beggTier\s*:',by_dex[row['dex']]['extensions']['upstreamRawRecord']['value'])
            self.assertEqual(row['declared'],bool(declaration))
        header=ROOT/'project/generated/include/content/EggContentPolicy.hpp'
        self.assertEqual(hashlib.sha256(header.read_bytes()).hexdigest(),report['generatedSHA256'])
    def test_determinism(self):
        paths=[ROOT/'project/generated/include/content/EggContentPolicy.hpp',ROOT/'docs/generated/EGG_CONTENT_IMPORT_REPORT.json']
        prepare();first=[p.read_bytes() for p in paths]
        prepare();self.assertEqual(first,[p.read_bytes() for p in paths])
if __name__=='__main__': unittest.main()
