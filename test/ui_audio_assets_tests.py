"""Verify pinned WAV -> PCM bytes, sample geometry and deterministic imports."""
import hashlib,importlib.util,io,json,subprocess,wave
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("ui_audio",root/"scripts/prepare_ui_audio.py")
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
report_path=root/"docs/generated/UI_AUDIO_IMPORT_REPORT.json"
header_path=root/"project/generated/include/content/NativeUiSounds.hpp"
report=json.loads(report_path.read_text(encoding="utf-8"))
assert report["revision"]==module.REVISION
assert [r["key"] for r in report["files"]]==["select","error","menu_open"]
for row in report["files"]:
    wav=subprocess.check_output(["git","-C",str(root/"build/upstream/pokerogue-assets"),"show",report["revision"]+":"+row["sourcePath"]])
    assert hashlib.sha256(wav).hexdigest()==row["sourceSHA256"]
    with wave.open(io.BytesIO(wav)) as sound:
        assert sound.getnchannels()==row["channels"]==2
        assert sound.getsampwidth()==2
        assert sound.getnframes()==row["frames"]
        assert sound.getframerate()==row["rate"]
        pcm=sound.readframes(row["frames"])
    physical=root/"build"/row["runtimePath"].replace("romfs:/","romfs/")
    assert pcm==physical.read_bytes()
    assert len(pcm)==row["bytes"]==row["frames"]*4
    assert hashlib.sha256(pcm).hexdigest()==row["pcmSHA256"]
before=(header_path.read_bytes(),report_path.read_bytes())
for _ in range(2):
    module.prepare(root)
    assert before==(header_path.read_bytes(),report_path.read_bytes())
backend=(root/"project/src/runtime/NativeUiAudio.cpp").read_text(encoding="utf-8")
assert backend.index("ndspExit();")<backend.index("linearFree(clip.pcm)")
assert "DSP_FlushDataCache(clip.pcm,definition.bytes)" in backend
assert "ndspChnWaveBufAdd(0,&buffer)" in backend
main=(root/"project/src/main.cpp").read_text(encoding="utf-8")
assert main.index("uiAudio.fini();")<main.index("    renderer.fini();",main.index("uiAudio.fini();"))
print("PASS: 3 pinned UI sounds, exact PCM samples, provenance and reproducible generation; NDSP execution pending")
