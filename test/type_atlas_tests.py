import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
from type_badges import atlas_frames
class TypeAtlasTests(unittest.TestCase):
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
