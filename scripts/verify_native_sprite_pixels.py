"""Verify every production sprite against pinned input and explicit pixel overrides."""
import json
import struct
import subprocess
import sys
import tempfile
from pathlib import Path
from PIL import Image
from native_sprite_pixels import ROOT, REPOSITORY, REVISION, adapt_atlas, inside, sha


def main():
    staged=json.loads((ROOT/'build/upstream-assets/staged-sprite-assets.json').read_text())
    converted=json.loads((ROOT/'build/upstream-assets/converted-sprite-assets.json').read_text())
    policy=json.loads((ROOT/'project/data/assets/presentation-overrides.json').read_text())['pokemonNativePixels']
    for inventory in (staged,converted):
        if inventory['repository']!=REPOSITORY or inventory['revision']!=REVISION:
            raise ValueError('Unexpected production source revision')
    index={a['atlasKey']+':'+a['facing']:a for a in converted['assets']}
    if len(index)!=len(staged['assets']) or len(index)!=len(converted['assets']):
        raise ValueError('Incomplete or duplicate sprite inventory')
    frames=adjusted=pages=0
    for source in staged['assets']:
        identity=source['atlasKey']+':'+source['facing'];runtime=index[identity]
        raw=inside(source['metadataPath']).read_bytes()
        metadata=inside(runtime['metadataPath']).read_bytes()
        if sha(raw)!=source['metadataSha256'] or sha(metadata)!=runtime['metadataSha256']:
            raise ValueError('Metadata checksum mismatch: '+identity)
        if metadata[20:84]!=raw[20:84]: raise ValueError('Lost source provenance: '+identity)
        count=struct.unpack_from('<I',metadata,16)[0]
        if count!=source['frameCount'] or len(metadata)!=84+32*count:
            raise ValueError('Lost animation frames: '+identity)
        budget=policy[source['facing']]
        width,height=struct.unpack_from('<HH',metadata,12)
        version=struct.unpack_from('<I',metadata,8)[0]
        if (metadata[:8],version) not in ((b'P3ATLAS1',1),(b'P3ATLAS2',2)):
            raise ValueError('Invalid runtime metadata signature: '+identity)
        if not 1<=len(runtime['textures'])<=4 or not 1<=width<=1024 or not 1<=height<=1024:
            raise ValueError('Texture exceeds runtime page/edge budget: '+identity)
        for i in range(count):
            offset=84+i*32
            x,y,w,h,sw,sh,tx,ty,duration,flags=struct.unpack_from('<10H',metadata,offset+12)
            if metadata[offset:offset+12]!=raw[offset:offset+12] or duration!=struct.unpack_from('<H',raw,offset+28)[0]:
                raise ValueError('Changed frame identity/duration: '+identity)
            page=flags>>1
            if page>=len(runtime['textures']) or x+w>width or y+h>height or not min(w,h,sw,sh):
                raise ValueError('Invalid runtime frame/page: '+identity)
            if sw>budget['width'] or sh>budget['height'] or tx+w>sw+1 or ty+h>sh+1:
                raise ValueError('Canvas/trim exceeds native pixel budget: '+identity)
        for texture in runtime['textures']:
            if sha(inside(texture['path']).read_bytes())!=texture['sha256']:
                raise ValueError('Texture checksum mismatch: '+identity)
        png=inside(source['sourcePath']).read_bytes()
        if sha(png)!=source['expectedSha256'].removeprefix('sha256:'):
            raise ValueError('Source PNG checksum mismatch: '+identity)
        with Image.open(inside(source['sourcePath'])) as image:
            expected=adapt_atlas(image.convert('RGBA'),raw,budget['width'],budget['height'])
        if expected is None:
            if runtime.get('nativeAdjustment') is not None:
                raise ValueError('Unexpected native override: '+identity)
        else:
            expected_pages,expected_metadata,adjustment=expected
            if metadata!=expected_metadata or runtime.get('nativeAdjustment')!=adjustment:
                raise ValueError('Non-reproducible native metadata: '+identity)
            if len(expected_pages)!=len(runtime['textures']): raise ValueError('Lost native texture page: '+identity)
            for n,page in enumerate(expected_pages):
                derived=ROOT/'build/upstream-assets/native-pages'/source['facing']/f"{source['atlasKey']}-p{n}.png"
                with Image.open(derived) as physical:
                    if physical.size!=page.size or physical.convert('RGBA').tobytes()!=page.tobytes():
                        raise ValueError('Native page differs from nearest source pixels: '+identity)
            adjusted+=1
        frames+=count;pages+=len(runtime['textures'])
    compiler='C:/devkitPro/msys2/usr/bin/g++.exe' if sys.platform=='win32' else 'g++'
    with tempfile.TemporaryDirectory() as directory:
        target=Path(directory);paths=target/'metadata-paths.txt'
        paths.write_text('\n'.join(inside(a['metadataPath']).as_posix() for a in converted['assets'])+'\n',encoding='utf-8',newline='\n')
        executable=target/('metadata.exe' if sys.platform=='win32' else 'metadata')
        subprocess.run([compiler,'-std=c++17','-O2','-I'+str(ROOT/'project/include'),'-I'+str(ROOT/'project/generated/include'),str(ROOT/'test/native/pokemon_atlas_metadata_harness.cpp'),str(ROOT/'project/src/runtime/PokemonAtlasMetadata.cpp'),'-o',str(executable)],check=True)
        subprocess.run([str(executable),'--catalog',str(paths)],check=True)
    print(f'PASS: {len(index)} pinned atlases, {frames} frames, {pages} physical textures, {adjusted} reproducible nearest overrides')


if __name__=='__main__': main()
