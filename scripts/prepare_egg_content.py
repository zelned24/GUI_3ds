"""Import pinned egg identifiers and incubation policy; no RNG or gameplay execution."""
import hashlib
import json
import re
import subprocess
from pathlib import Path
from prepare_nature_ui import GAME_REV, ROOT

REPOSITORY = 'https://github.com/pagefaultgames/pokerogue'
def prepare(root=ROOT):
    paths=['src/enums/egg-type.ts','src/enums/egg-source-types.ts','src/enums/voucher-type.ts','src/data/balance/rates.ts','src/data/egg.ts','src/phases/egg-lapse-phase.ts']
    raw={p:subprocess.check_output(['git','-C',str(root/'build/upstream/pokerogue'),'show',GAME_REV+':'+p]) for p in paths}
    enums={}
    for path,symbol in zip(paths[:3],['EggTier','EggSourceType','VoucherType']):
        script="import {PokerogueEnumParser} from './tools/js/data/PokerogueEnumParser.js'; const p=new PokerogueEnumParser().parseEnum("+json.dumps(raw[path].decode('utf-8'))+","+json.dumps(symbol)+","+json.dumps(path)+"); console.log(JSON.stringify(Object.fromEntries(p.idToSymbol)));"
        enums[symbol]=[{'id':int(k),'symbol':v} for k,v in sorted(json.loads(subprocess.check_output(['node','--input-type=module','-e',script],cwd=root)).items(),key=lambda x:int(x[0]))]
    rates=raw[paths[3]].decode('utf-8')
    constants={}
    for suffix in ['COMMON','RARE','EPIC','LEGENDARY','MANAPHY']:
        name='HATCH_WAVES_'+suffix+'_EGG'
        matches=re.findall(r'export const '+name+r'\s*=\s*(\d+)\s*;',rates)
        if len(matches)!=1: raise ValueError('Unsupported upstream incubation constant: '+name)
        constants[name]=int(matches[0])
    expected=['COMMON','RARE','EPIC','LEGENDARY']
    if [r['symbol'] for r in enums['EggTier']]!=expected: raise ValueError('Egg tier mapping requires review')
    header='// Generated from pinned egg enums and balance rates.\n#pragma once\n#include <cstdint>\nnamespace Pokerogue3DS {\n'
    for symbol,rows in enums.items():
        header+='enum class '+symbol+' : uint8_t {\n'+''.join('    '+r['symbol']+' = '+str(r['id'])+',\n' for r in rows)+'};\n'
    header+='struct EggIncubationPolicy { EggTier tier; uint16_t waves; };\ninline constexpr EggIncubationPolicy kEggIncubationPolicies[]={\n'
    header+=''.join('    {EggTier::'+r['symbol']+','+str(constants['HATCH_WAVES_'+r['symbol']+'_EGG'])+'},\n' for r in enums['EggTier'])+'};\n'
    header+='inline constexpr uint16_t kManaphyEggHatchWaves='+str(constants['HATCH_WAVES_MANAPHY_EGG'])+';\n}\n'
    target=root/'project/generated/include/content/EggContentPolicy.hpp';target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(header.encode('utf-8'))
    report={'schemaVersion':1,'sources':[{'repository':REPOSITORY,'revision':GAME_REV,'sourcePath':p,'sourceSymbol':{'src/enums/egg-type.ts':'EggTier','src/enums/egg-source-types.ts':'EggSourceType','src/enums/voucher-type.ts':'VoucherType','src/data/balance/rates.ts':'HATCH_WAVES_*_EGG','src/data/egg.ts':'Egg.getEggTierDefaultHatchWaves','src/phases/egg-lapse-phase.ts':'EggLapsePhase.start'}[p],'schemaVersion':1,'hash':hashlib.sha256(raw[p]).hexdigest()} for p in paths],'enums':enums,'incubationConstants':constants,'scope':'IDENTIFIERS_AND_INCUBATION_CONSTANTS_ONLY','runtimeIntegration':'PENDING_INVENTORY_GACHA_HATCHING','generatedSHA256':hashlib.sha256(target.read_bytes()).hexdigest()}
    destination=root/'docs/generated/EGG_CONTENT_IMPORT_REPORT.json';destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes((json.dumps(report,ensure_ascii=False,sort_keys=True,indent=2)+'\n').encode('utf-8'))
    return report
if __name__=='__main__':
    print(prepare()['generatedSHA256'])
