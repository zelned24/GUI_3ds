"""Import pinned egg presentation text and incubation message thresholds."""
import hashlib
import json
import re
import subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
GAME_REV = "8555c08c823b856cbec4eb99ca84ea52a955836d"
LOCALE_REV = "23aea1cb0da5a0b15b836f3c243791591cc42303"
def prepare(root=ROOT):
    paths = [("pokerogue", GAME_REV, "src/data/egg.ts"),
             ("pokerogue-locales", LOCALE_REV, "es-ES/egg.json")]
    raw = [subprocess.check_output(["git", "-C", str(root/"build/upstream"/repo),
           "show", revision+":"+path]) for repo, revision, path in paths]
    source = raw[0].decode("utf-8")
    body = source.split("public getEggHatchWavesMessage(): string {", 1)[1].split("public getEggTypeDescriptor", 1)[0]
    thresholds = re.findall(r'this.hatchWaves <= (\d+)\) \{\s*return i18next.t\("egg:([^"\n]+)"\)', body)
    if len(thresholds) != 3: raise ValueError("Unsupported upstream egg message policy")
    limits=[int(limit) for limit,_ in thresholds]
    if limits!=sorted(set(limits)): raise ValueError("Invalid egg message thresholds")
    fallback = re.findall(r'return i18next.t\("egg:([^"\n]+)"\)', body)[-1]
    text = json.loads(raw[1])
    if fallback not in text: raise ValueError("Missing fallback egg locale")
    if not all(isinstance(value, str) for value in text.values()): raise ValueError("Unsupported egg locale")
    header = '// Generated pinned egg presentation.\n#pragma once\n#include <cstdint>\n#include <cstring>\nnamespace Pokerogue3DS {\n'
    header += 'struct EggUiTextEntry {const char* key;const char* text;};\ninline constexpr EggUiTextEntry kEggUiTexts[]={\n'
    header += '\n'.join('    {'+json.dumps(key)+','+json.dumps(value,ensure_ascii=False)+'},' for key,value in sorted(text.items()))
    header += '\n};\ninline const char* eggUiText(const char* key) {for(const auto& row:kEggUiTexts) if(key && !std::strcmp(row.key,key)) return row.text;return key;}\n'
    header += 'inline const char* eggHatchMessageKey(int32_t waves) {\n'
    for threshold,key in thresholds:
        if key not in text: raise ValueError("Missing egg message locale")
        header += '    if(waves<='+threshold+') return '+json.dumps(key)+';\n'
    header += '    return '+json.dumps(fallback)+';\n}\n}\n'
    target=root/"project/generated/include/content/EggUiText.hpp"
    target.write_bytes(header.encode("utf-8"))
    report={"schemaVersion":1,"sources":[{"repository":"https://github.com/pagefaultgames/"+repo,"revision":rev,"sourcePath":path,"sourceSHA256":hashlib.sha256(data).hexdigest()} for (repo,rev,path),data in zip(paths,raw)],"thresholds":thresholds,"generatedSHA256":hashlib.sha256(target.read_bytes()).hexdigest()}
    (root/"docs/generated/EGG_UI_IMPORT_REPORT.json").write_bytes((json.dumps(report,sort_keys=True,indent=2)+"\n").encode("utf-8"))
    return report
if __name__=="__main__": print(prepare()["generatedSHA256"])
