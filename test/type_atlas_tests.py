import copy,sys,unittest,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
from type_badges import atlas_frames
from hud_type_icons import validate_size,REVISION
class TypeAtlasTests(unittest.TestCase):
    def test_explicit_size_override_is_hash_and_revision_bound(self):
        atlas={"size":{"w":96,"h":12}}
        png=b"physical PNG";manifest=b"original JSON"
        row={"revision":REVISION,"sourceSHA256":hashlib.sha256(manifest).hexdigest(),"imageSHA256":hashlib.sha256(png).hexdigest(),"declaredSize":{"w":96,"h":12},"physicalSize":{"w":86,"h":12}}
        overrides={"boss.json":row}
        self.assertEqual(validate_size(atlas,86,12,"boss.json",png,manifest,overrides),row)
        self.assertEqual(atlas["size"],{"w":96,"h":12})
        self.assertIsNone(validate_size(atlas,96,12,"boss.json",png,manifest,{}))
        for path,image,meta,over in [("other.json",png,manifest,overrides),("boss.json",png+b"changed",manifest,overrides),("boss.json",png,manifest+b"changed",overrides),("boss.json",png,manifest,{"boss.json":dict(row,revision="changed")})]:
            with self.assertRaises(ValueError):validate_size(atlas,86,12,path,image,meta,over)

    def test_bounds_trim_and_sorted_keys(self):
        f={"filename":"fire","rotated":False,"frame":{"x":0,"y":0,"w":20,"h":12},"sourceSize":{"w":23,"h":18},"spriteSourceSize":{"x":0,"y":6}}
        atlas={"frames":[dict(f,filename="water"),f]}
        self.assertEqual(atlas_frames(atlas,20,24),[("fire",[0,0,20,12,23,18,0,6]),("water",[0,0,20,12,23,18,0,6])])
        self.assertEqual(atlas_frames(atlas,20,24),atlas_frames(atlas,20,24))
        changes=[("rotated",True),("filename","bad/key"),("frame",dict(f["frame"],x=1)),("frame",dict(f["frame"],y=-1)),("frame",dict(f["frame"],w=0)),("spriteSourceSize",{"x":4,"y":6}),("spriteSourceSize",{"x":0,"y":7})]
        for key,value in changes:
            with self.subTest(key=key,value=value):
                bad=copy.deepcopy(f);bad[key]=value
                with self.assertRaises(ValueError):atlas_frames({"frames":[bad]},20,24)
        with self.assertRaises(ValueError):atlas_frames({"frames":[f,f]},20,24)
if __name__=="__main__":unittest.main()
