import hashlib
import importlib.util
import json
import subprocess
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('nature_ui',ROOT/'scripts/prepare_nature_ui.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class NatureUiGenerationTests(unittest.TestCase):
    def test_pinned_labels_and_provenance(self):
        report=module.prepare()
        self.assertEqual([row['id'] for row in report['rows']],list(range(25)))
        for source in report['sources']:
            checkout='pokerogue-locales' if 'locales' in source['repository'] else 'pokerogue'
            raw=subprocess.check_output(['git','-C',str(ROOT/'build/upstream'/checkout),'show',source['revision']+':'+source['sourcePath']])
            self.assertEqual(hashlib.sha256(raw).hexdigest(),source['sourceSHA256'])
            if checkout=='pokerogue-locales':
                labels=json.loads(raw)
                for row in report['rows']:
                    self.assertEqual(row['name'],labels[row['symbol'].lower()])
        header=ROOT/'project/generated/include/content/NatureUiNames.hpp'
        self.assertEqual(hashlib.sha256(header.read_bytes()).hexdigest(),report['generatedSHA256'])

    def test_repeated_generation_is_byte_identical(self):
        paths=[ROOT/'project/generated/include/content/NatureUiNames.hpp',ROOT/'docs/generated/NATURE_UI_IMPORT_REPORT.json']
        module.prepare();first=[p.read_bytes() for p in paths]
        module.prepare();self.assertEqual(first,[p.read_bytes() for p in paths])

if __name__=='__main__': unittest.main()
