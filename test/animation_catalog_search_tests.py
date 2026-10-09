"""Offline playback-bound equivalence over physical converted Pokemon atlases.
This validates metadata and the algorithm model, not compiled C++ or the GPU.
"""
from bisect import bisect_right
import hashlib,json,struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
files=sorted((ROOT/"build/romfs/sprites/pokemon/atlas").rglob("*.p3a"))
assert files,"Physical production atlas metadata is required"
source=ROOT/"project/src/runtime/PokemonAtlasMetadata.cpp"
assert "std::upper_bound(m_animationIndices.begin()" in source.read_text(encoding="utf-8")
combined=hashlib.sha256();frames=checks=0
for path in files:
    raw=path.read_bytes()
    assert raw[:8] in (b"P3ATLAS1",b"P3ATLAS2")
    count=struct.unpack_from("<I",raw,16)[0]
    assert count and count<=1024 and len(raw)==84+count*32
    names=[raw[84+i*32:96+i*32].split(b"\0",1)[0].decode("ascii") for i in range(count)]
    # Match load(): choose the first packed frame for each numeric name.
    first={}
    for i,name in enumerate(names):first.setdefault(name,i)
    ordered=[first[f"{n:04d}.png"] for n in range(1,401) if f"{n:04d}.png" in first]
    keys=[names[i] for i in ordered]
    cursor=0
    for limit in range(1,401):
        bound=f"{limit:04d}.png"
        while cursor<len(keys) and keys[cursor]<=bound:cursor+=1
        assert bisect_right(keys,bound)==cursor
        checks+=1
    for limit in (1,2,128,400):
        bound=f"{limit:04d}.png"
        reference=[i for i in ordered if names[i]<=bound]
        actual=ordered[:bisect_right(keys,bound)]
        assert actual==reference
        for rate in (10,24):
            for time in (0,41,42,999,1000,(1<<64)-1):
                n=len(reference)
                expected=reference[((time//1000%n)*rate+(time%1000)*rate//1000)%n] if n else 0
                n=len(actual)
                result=actual[((time//1000%n)*rate+(time%1000)*rate//1000)%n] if n else 0
                assert result==expected
                checks+=1
    relative=path.relative_to(ROOT).as_posix()
    combined.update(relative.encode()+b"\0"+hashlib.sha256(raw).digest())
    frames+=count
report=dict(schemaVersion=1,scope="OFFLINE_METADATA_AND_ALGORITHM_MODEL",nativeExecution="NOT_EXECUTED",gpuExecution="NOT_EXECUTED",atlasCount=len(files),frameCount=frames,comparisonCount=checks,metadataHash=combined.hexdigest(),runtimeSourcePath=source.relative_to(ROOT).as_posix(),runtimeSourceSHA256=hashlib.sha256(source.read_bytes()).hexdigest())
(ROOT/"docs/generated/ANIMATION_SEARCH_AUDIT.json").write_text(json.dumps(report,sort_keys=True,indent=2)+"\n",encoding="utf-8",newline="\n")
print(f"PASS offline animation search: {len(files)} atlases, {frames} frames, {checks} comparisons; C++/GPU pending")
