"""Materialize pinned 40x30 icon tiles without downsampling; no game compilation."""
import hashlib,io,json,subprocess,zipfile
from pathlib import Path
from PIL import Image
from inventory_pokemon_icons import inventory
ROOT=Path(__file__).resolve().parents[1]
def prepare(root=ROOT):
    source=inventory(root)
    archive=subprocess.check_output(['git','-C',str(root/'build/upstream/pokerogue-assets'),'archive','--format=zip',source['revision'],source['sourceRoot']])
    staged=root/'build/native-presentation/appearance-icon-tiles';staged.mkdir(parents=True,exist_ok=True)
    output=root/'build/romfs/presentation/icon-tiles';output.mkdir(parents=True,exist_ok=True)
    rows=[]
    with zipfile.ZipFile(io.BytesIO(archive)) as files:
        for index,row in enumerate(source['files']):
            raw=files.read(row['sourcePath'])
            if hashlib.sha256(raw).hexdigest()!=row['sourceSHA256']: raise ValueError('Pinned icon checksum mismatch')
            with Image.open(io.BytesIO(raw)) as original:
                image=original.convert('RGBA')
            if image.size!=(40,30): raise ValueError('Unsupported native icon dimensions')
            canvas=Image.new('RGBA',(64,32));canvas.paste(image,(0,0))
            png=staged/f'{index}.png';texture=output/f'{index}.t3x'
            encoded=io.BytesIO();canvas.save(encoded,format='PNG');png_bytes=encoded.getvalue()
            unchanged=png.exists() and png.read_bytes()==png_bytes and texture.exists()
            png.write_bytes(png_bytes)
            if not unchanged: subprocess.run(['C:/devkitPro/tools/bin/tex3ds.exe','-f','rgba8','-o',str(texture),str(png)],check=True,stdout=subprocess.DEVNULL)
            rows.append({'sourcePath':row['sourcePath'],'sourceSHA256':row['sourceSHA256'],'runtimePath':f'romfs:/presentation/icon-tiles/{index}.t3x','stagedSHA256':hashlib.sha256(png_bytes).hexdigest(),'convertedSHA256':hashlib.sha256(texture.read_bytes()).hexdigest(),'width':40,'height':30,'textureWidth':64,'textureHeight':32,'estimatedResidentBytes':64*32*4})
    report={'schemaVersion':1,'repository':source['repository'],'revision':source['revision'],'sourceInventoryHash':source['contentSHA256'],'adaptation':'ORIGINAL_RGBA_PIXELS_1_TO_1_PADDED_64_32','runtimeValidation':'NOT_EXECUTED','files':rows}
    (root/'docs/generated/APPEARANCE_ICON_TILE_REPORT.json').write_bytes((json.dumps(report,sort_keys=True,indent=2)+'\n').encode())
    header='// Generated native pinned icon tiles, same ordering as AppearanceIcons.hpp.\n#pragma once\n#include <cstddef>\nnamespace Pokerogue3DS {\ninline constexpr const char* kAppearanceIconTiles[]={\n'+''.join(json.dumps(row['runtimePath'])+',\n' for row in rows)+'};\n}\n'
    (root/'project/generated/include/content/AppearanceIconTiles.hpp').write_bytes(header.encode())
    print('Generated',len(rows),'native icon tiles; visible 18 texture bytes:',18*64*32*4)
    return report
if __name__=='__main__': prepare()
