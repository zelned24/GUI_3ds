"""Extract pinned, unmodified stereo PCM16 UI sounds for libctru NDSP."""
import hashlib,io,json,subprocess,wave
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
REVISION="056a1f408f26a3be4fef243f7462cb43608c7928"
def prepare(root=ROOT):
    output=root/"build/romfs/audio/ui"
    output.mkdir(parents=True,exist_ok=True)
    rows=[]
    for key in ("select","error","menu_open"):
        source_path=f"audio/ui/{key}.wav"
        raw=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"show",REVISION+":"+source_path])
        with wave.open(io.BytesIO(raw)) as sound:
            if sound.getnchannels()!=2 or sound.getsampwidth()!=2 or sound.getcomptype()!="NONE":
                raise ValueError("Unsupported upstream UI WAV encoding: "+source_path)
            frames=sound.getnframes();rate=sound.getframerate()
            pcm=sound.readframes(frames)
        if not frames or len(pcm)!=frames*4 or len(pcm)>128*1024 or not 8000<=rate<=48000:
            raise ValueError("Invalid UI audio geometry: "+source_path)
        target=output/(key+".pcm")
        target.write_bytes(pcm)
        rows.append(dict(key=key,sourcePath=source_path,sourceSHA256=hashlib.sha256(raw).hexdigest(),runtimePath="romfs:/audio/ui/"+target.name,pcmSHA256=hashlib.sha256(pcm).hexdigest(),frames=frames,rate=rate,channels=2,bytes=len(pcm),format="STEREO_PCM16_LE"))
    header="// Generated from pinned UI WAV sources, no resampling.\n#pragma once\n#include <cstdint>\nnamespace Pokerogue3DS {\nstruct NativeUiSoundDefinition {const char* key;const char* path;uint32_t frames,rate,bytes;};\ninline constexpr NativeUiSoundDefinition kNativeUiSounds[]={\n"
    header+="\n".join("    {"+json.dumps(r["key"])+","+json.dumps(r["runtimePath"])+f',{r["frames"]},{r["rate"]},{r["bytes"]}' +"}," for r in rows)
    header+="\n};\n}\n"
    (root/"project/generated/include/content/NativeUiSounds.hpp").write_bytes(header.encode())
    report=dict(schemaVersion=1,repository="https://github.com/pagefaultgames/pokerogue-assets",revision=REVISION,files=rows)
    (root/"docs/generated/UI_AUDIO_IMPORT_REPORT.json").write_bytes((json.dumps(report,sort_keys=True,indent=2)+"\n").encode())
    return report
if __name__=="__main__": print("Imported",len(prepare()["files"]),"UI sounds")
