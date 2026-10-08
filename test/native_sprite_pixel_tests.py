import struct
import sys
import unittest
import subprocess
import tempfile
from pathlib import Path
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
from native_sprite_pixels import adapt_atlas

class NativeSpriteTests(unittest.TestCase):
    def source(self):
        image=Image.new("RGBA",(24,8));image.paste((255,0,0,255),(0,0,12,8));image.paste((0,0,255,255),(12,0,24,8))
        data=bytearray(148);data[:8]=b"P3ATLAS1";struct.pack_into("<IHHI",data,8,1,24,8,2)
        for i in range(2):
            p=84+i*32;data[p:p+8]=f"000{i+1}.png".encode()
            struct.pack_into("<10H",data,p+12,i*12,0,12,8,12,8,0,0,100+i,0)
        return image,bytes(data)
    def test_resample_names_duration_hashes_and_pixels(self):
        image,raw=self.source();pages,metadata,adjustment=adapt_atlas(image,raw,6,4)
        self.assertEqual(adjustment["runtimeCanvas"],[6,4]);self.assertEqual(len(pages),1)
        self.assertEqual(metadata[:8],b"P3ATLAS2");self.assertEqual(metadata[20:84],raw[20:84])
        self.assertEqual(struct.unpack_from("<HH",metadata,12),pages[0].size)
        for i in range(2):
            p=84+i*32;self.assertEqual(raw[p:p+12],metadata[p:p+12])
            x,y,w,h,sw,sh,tx,ty,duration,flags=struct.unpack_from("<10H",metadata,p+12)
            self.assertEqual((sw,sh,duration),(6,4,100+i))
            expected=image.crop((i*12,0,i*12+12,8)).resize((6,4),Image.Resampling.NEAREST)
            self.assertEqual(pages[flags>>1].crop((x,y,x+w,y+h)).tobytes(),expected.tobytes())
        self.assertEqual(adapt_atlas(image,raw,6,4)[1],metadata)
    def test_native_size_does_not_rewrite_upstream(self):
        image,raw=self.source();self.assertIsNone(adapt_atlas(image,raw,12,8))
    def test_invalid_data_fail_clearly(self):
        image,raw=self.source()
        for pos in (0,16,96,100,108,110):
            bad=bytearray(raw);bad[pos]=255
            with self.assertRaises(ValueError): adapt_atlas(image,bytes(bad),6,4)
        with self.assertRaises(ValueError): adapt_atlas(image,raw,0,4)
    def test_transparency_and_common_canvas(self):
        image,raw=self.source();data=bytearray(raw)
        struct.pack_into("<H",data,84+32+20,13)
        pages,metadata,adjustment=adapt_atlas(image,bytes(data),6,4)
        self.assertEqual(adjustment["originalCanvas"],[13,8])
        self.assertEqual(struct.unpack_from("<HH",metadata,84+20),struct.unpack_from("<HH",metadata,116+20))
    def test_trimmed_frames_keep_common_bottom_center_anchor(self):
        image,raw=self.source();data=bytearray(raw)
        # Different source canvases and trim positions: reconstruct exactly
        # before nearest conversion rather than resizing each crop separately.
        struct.pack_into('<10H',data,84+12,0,0,12,8,16,12,2,3,100,0)
        struct.pack_into('<10H',data,116+12,12,0,12,8,18,14,3,4,101,0)
        pages,metadata,adjustment=adapt_atlas(image,bytes(data),9,7)
        self.assertEqual(adjustment['originalCanvas'],[18,14])
        self.assertEqual(adjustment['runtimeCanvas'],[9,7])
        for i,(cw,ch,tx,ty) in enumerate([(16,12,2,3),(18,14,3,4)]):
            common=Image.new('RGBA',(18,14))
            common.paste(image.crop((i*12,0,i*12+12,8)),((18-cw)//2+tx,14-ch+ty))
            expected=common.resize((9,7),Image.Resampling.NEAREST)
            x,y,w,h,sw,sh,trim_x,trim_y,duration,flags=struct.unpack_from('<10H',metadata,84+i*32+12)
            actual=Image.new('RGBA',(sw,sh))
            actual.paste(pages[flags>>1].crop((x,y,x+w,y+h)),(trim_x,trim_y))
            self.assertEqual(actual.tobytes(),expected.tobytes())
            self.assertEqual((sw,sh,duration),(9,7,100+i))
        self.assertEqual(adapt_atlas(image,bytes(data),9,7)[1],metadata)

    def test_cpp_runtime_reads_native_canvas_and_clears_failed_load(self):
        image,raw=self.source();data=bytearray(raw)
        struct.pack_into('<H',data,116+20,13)
        pages,metadata,adjustment=adapt_atlas(image,bytes(data),6,4)
        root=Path(__file__).resolve().parents[1]
        compiler='C:/devkitPro/msys2/usr/bin/g++.exe' if sys.platform=='win32' else 'g++'
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory);original=target/'source.p3a';native=target/'native.p3a'
            original.write_bytes(data);native.write_bytes(metadata)
            executable=target/('metadata.exe' if sys.platform=='win32' else 'metadata')
            subprocess.run([compiler,'-std=c++17','-O2','-I'+str(root/'project/include'),'-I'+str(root/'project/generated/include'),str(root/'test/native/pokemon_atlas_metadata_harness.cpp'),str(root/'project/src/runtime/PokemonAtlasMetadata.cpp'),'-o',str(executable)],check=True,capture_output=True)
            subprocess.run([str(executable),str(original),str(native)],check=True,capture_output=True)
if __name__=="__main__": unittest.main()
