"""Verify native icon tile provenance and original pixels; no runtime build."""
import hashlib,io,json,subprocess,zipfile
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
report=json.loads((ROOT/'docs/generated/APPEARANCE_ICON_TILE_REPORT.json').read_text(encoding='utf-8'))
archive=subprocess.check_output(['git','-C',str(ROOT/'build/upstream/pokerogue-assets'),'archive','--format=zip',report['revision'],'images/pokemon/icons'])
with zipfile.ZipFile(io.BytesIO(archive)) as files:
    assert sorted(row['sourcePath'] for row in report['files'])==sorted(name for name in files.namelist() if name.endswith('.png'))
    for index,row in enumerate(report['files']):
        raw=files.read(row['sourcePath']);assert hashlib.sha256(raw).hexdigest()==row['sourceSHA256']
        png=ROOT/'build/native-presentation/appearance-icon-tiles'/f'{index}.png'
        texture=ROOT/'build/romfs'/row['runtimePath'].removeprefix('romfs:/')
        assert hashlib.sha256(png.read_bytes()).hexdigest()==row['stagedSHA256']
        assert hashlib.sha256(texture.read_bytes()).hexdigest()==row['convertedSHA256']
        with Image.open(io.BytesIO(raw)) as original,Image.open(png) as staged:
            expected=original.convert('RGBA');actual=staged.convert('RGBA')
            assert actual.size==(64,32) and expected.size==(40,30)
            assert actual.crop((0,0,40,30)).tobytes()==expected.tobytes()
            assert not actual.crop((40,0,64,32)).getbbox()
            assert not actual.crop((0,30,64,32)).getbbox()
        assert row['estimatedResidentBytes']==8192
print('PASS original pinned pixels, transparent padding, provenance and conversion hashes:',len(report['files']))

header=(ROOT/'project/generated/include/content/AppearanceIconTiles.hpp').read_text(encoding='utf-8')
assert header.count('romfs:/presentation/icon-tiles/')==len(report['files'])
for row in report['files']: assert json.dumps(row['runtimePath'])+',' in header

import importlib.util,tempfile
spec=importlib.util.spec_from_file_location('icon_tiles',ROOT/'scripts/prepare_appearance_icon_tiles.py')
import sys
sys.path.insert(0,str(ROOT/'scripts'))
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
with tempfile.TemporaryDirectory() as directory:
    png=Path(directory)/'source.png';texture=Path(directory)/'icon.t3x'
    png.write_bytes(b'pixels');texture.write_bytes(b'converted')
    previous={'convertedSHA256':hashlib.sha256(b'converted').hexdigest()}
    assert module.cached_tile_valid(png,texture,b'pixels',previous)
    texture.write_bytes(b'corrupt')
    assert not module.cached_tile_valid(png,texture,b'pixels',previous)
    texture.write_bytes(b'converted')
    assert not module.cached_tile_valid(png,texture,b'changed',previous)
    assert not module.cached_tile_valid(png,texture,b'pixels',None)
    texture.unlink()
    assert not module.cached_tile_valid(png,texture,b'pixels',previous)
print('PASS cache reuse requires unchanged pixels and verified converted bytes')
