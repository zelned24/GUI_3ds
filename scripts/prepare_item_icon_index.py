"""Index existing converted individual item icons against pinned PNG sources."""
import hashlib,io,json,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
REVISION="056a1f408f26a3be4fef243f7462cb43608c7928"
def prepare(root=ROOT):
    archive=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"archive","--format=zip",REVISION,"images/items"])
    rows=[]
    with zipfile.ZipFile(io.BytesIO(archive)) as sources:
        for name in sorted(sources.namelist()):
            if not name.endswith(".png"): continue
            path=Path(name)
            if path.parent.as_posix()!="images/items": raise ValueError("Unsupported item source path")
            raw=sources.read(name)
            from PIL import Image
            with Image.open(io.BytesIO(raw)) as image: width,height=image.size
            target=root/"build/romfs/presentation/items"/(path.stem+".t3x")
            if not target.is_file(): raise ValueError("Missing converted item: "+str(target))
            rows.append({"key":path.stem,"sourcePath":name,"sourceSHA256":hashlib.sha256(raw).hexdigest(),"width":width,"height":height,"runtimePath":"romfs:/presentation/items/"+target.name,"convertedSHA256":hashlib.sha256(target.read_bytes()).hexdigest()})
    rows.sort(key=lambda row:row["key"])
    header='// Generated physical pinned item icon paths.\n#pragma once\n#include <cstdint>\n#include <cstring>\nnamespace Pokerogue3DS {\nstruct ItemIconTexture {const char* key;const char* path;uint16_t width,height;};\ninline constexpr ItemIconTexture kItemIconTextures[]={\n'
    header+='\n'.join('    {'+json.dumps(row['key'])+','+json.dumps(row['runtimePath'])+','+str(row['width'])+','+str(row['height'])+'},' for row in rows)
    header+='\n};\ninline const ItemIconTexture* findItemIconTexture(const char* key) {if(!key) return nullptr;unsigned first=0,last=sizeof(kItemIconTextures)/sizeof(kItemIconTextures[0]);while(first<last) {const unsigned mid=first+(last-first)/2;const int order=std::strcmp(kItemIconTextures[mid].key,key);if(!order) return &kItemIconTextures[mid];if(order<0) first=mid+1;else last=mid;}return nullptr;}\n}\n'
    (root/'project/generated/include/content/ItemIconTextures.hpp').write_bytes(header.encode('utf-8'))
    report={"schemaVersion":1,"repository":"https://github.com/pagefaultgames/pokerogue-assets","revision":REVISION,"files":rows}
    (root/'docs/generated/ITEM_ICON_TEXTURE_REPORT.json').write_bytes((json.dumps(report,sort_keys=True,indent=2)+'\n').encode('utf-8'))
    return report
if __name__=='__main__': print('Indexed',len(prepare()['files']),'physical item textures')
