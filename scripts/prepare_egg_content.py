"""Import pinned egg identifiers and incubation policy; no RNG or gameplay execution."""
import hashlib
import json
import re
import subprocess
from pathlib import Path
from prepare_nature_ui import GAME_REV, ROOT

REPOSITORY = 'https://github.com/pagefaultgames/pokerogue'
def prepare(root=ROOT):
    paths=['src/enums/egg-type.ts','src/enums/egg-source-types.ts','src/enums/voucher-type.ts','src/data/balance/rates.ts','src/data/egg.ts','src/phases/egg-lapse-phase.ts','src/system/egg-data.ts','src/enums/variant-tier.ts','src/enums/species-id.ts','src/data/species-data-registry.ts','src/ui/handlers/egg-gacha-ui-handler.ts']
    raw={p:subprocess.check_output(['git','-C',str(root/'build/upstream/pokerogue'),'show',GAME_REV+':'+p]) for p in paths}
    enums={}
    for path,symbol in zip(paths[:3]+[paths[7],paths[8]],['EggTier','EggSourceType','VoucherType','VariantTier','SpeciesId']):
        script="import {PokerogueEnumParser} from './tools/js/data/PokerogueEnumParser.js'; const p=new PokerogueEnumParser().parseEnum("+json.dumps(raw[path].decode('utf-8'))+","+json.dumps(symbol)+","+json.dumps(path)+"); console.log(JSON.stringify(Object.fromEntries(p.idToSymbol)));"
        enums[symbol]=[{'id':int(k),'symbol':v} for k,v in sorted(json.loads(subprocess.check_output(['node','--input-type=module'],input=script.encode('utf-8'),cwd=root)).items(),key=lambda x:int(x[0]))]
    species_ids={r['symbol']:r['id'] for r in enums.pop('SpeciesId')}
    egg_source=raw['src/data/egg.ts'].decode('utf-8')
    match=re.search(r'private getEggTierDefaultHatchWaves\(eggTier\?: EggTier\): number\s*\{\s*if\s*\((.*?)\)\s*\{\s*return HATCH_WAVES_MANAPHY_EGG;',egg_source,re.S)
    if not match: raise ValueError('Unsupported special-species incubation policy')
    special_names=re.findall(r'this\._species === SpeciesId\.([A-Z0-9_]+)',match[1])
    if not special_names or re.sub(r'this\._species === SpeciesId\.[A-Z0-9_]+|\|\||\s','',match[1]):
        raise ValueError('Unsupported special-species incubation predicate')
    special_species=[{'symbol':name,'id':species_ids[name]} for name in special_names]
    rates=raw[paths[3]].decode('utf-8')
    constants={}
    for suffix in ['COMMON','RARE','EPIC','LEGENDARY','MANAPHY']:
        name='HATCH_WAVES_'+suffix+'_EGG'
        matches=re.findall(r'export const '+name+r'\s*=\s*(\d+)\s*;',rates)
        if len(matches)!=1: raise ValueError('Unsupported upstream incubation constant: '+name)
        constants[name]=int(matches[0])
        if not 0<constants[name]<=65535: raise ValueError('Incubation duration exceeds runtime storage')
    gacha_constants={}
    for name in ['GACHA_DEFAULT_COMMON_EGG_THRESHOLD','GACHA_DEFAULT_RARE_EGG_THRESHOLD','GACHA_DEFAULT_EPIC_EGG_THRESHOLD','GACHA_LEGENDARY_UP_THRESHOLD_OFFSET']:
        matches=re.findall(r'export const '+name+r'\s*=\s*(\d+)\s*;',rates)
        if len(matches)!=1: raise ValueError('Unsupported gacha threshold: '+name)
        gacha_constants[name]=int(matches[0])
    common,rare,epic,offset=gacha_constants.values()
    if not 0<epic<rare<common<256 or common+offset>=256:
        raise ValueError('Gacha threshold ordering requires explicit runtime adaptation')
    pity_constants={}
    for name in ['EGG_PITY_RARE_THRESHOLD','EGG_PITY_EPIC_THRESHOLD','EGG_PITY_LEGENDARY_THRESHOLD']:
        matches=re.findall(r'export const '+name+r'\s*=\s*(\d+)\s*;',rates)
        if len(matches)!=1 or not 0<int(matches[0])<=4294967295:
            raise ValueError('Unsupported egg pity threshold: '+name)
        pity_constants[name]=int(matches[0])
    gacha_source=raw['src/ui/handlers/egg-gacha-ui-handler.ts'].decode('utf-8')
    offer_switch=re.search(r'private static cursorToVoucher\b[\s\S]*?switch \(cursor\)\s*\{(.*?)\n\s*\}\n\s*\}',gacha_source,re.S)
    if not offer_switch: raise ValueError('Unsupported voucher selector')
    offer_pattern=r'case\s+(\d+):\s*return\s*\[VoucherType\.([A-Z_]+),\s*(\d+),\s*(\d+)\];'
    offers=[{'cursor':int(m[0]),'voucher':m[1],'consumed':int(m[2]),'pulls':int(m[3])} for m in re.findall(offer_pattern,offer_switch[1])]
    if re.sub(offer_pattern,'',offer_switch[1]).strip() or not offers or [r['cursor'] for r in offers]!=list(range(len(offers))):
        raise ValueError('Unsupported voucher options')
    if len(offers)>255 or any(not 0<r['consumed']<=65535 or not 0<r['pulls']<=65535 for r in offers):
        raise ValueError('Voucher options exceed runtime representation')
    limit=re.findall(r'globalScene\.gameData\.eggs\.length \+ pulls > (\d+)',gacha_source)
    if len(limit)!=1: raise ValueError('Unsupported egg inventory pull limit')
    inventory_limit=int(limit[0])
    if not 0<inventory_limit<=4294967295: raise ValueError('Invalid inventory limit')
    if enums['VoucherType']!=[{'id':i,'symbol':name} for i,name in enumerate(['REGULAR','PLUS','PREMIUM','GOLDEN'])]:
        raise ValueError('Voucher ledger schema requires explicit adaptation')
    if any(r['voucher'] not in {'REGULAR','PLUS','PREMIUM','GOLDEN'} for r in offers):
        raise ValueError('Unknown voucher offer type')
    expected=['COMMON','RARE','EPIC','LEGENDARY']
    if [r['symbol'] for r in enums['EggTier']]!=expected: raise ValueError('Egg tier mapping requires review')
    header='// Generated from pinned egg enums and balance rates.\n#pragma once\n#include <cstdint>\nnamespace Pokerogue3DS {\n'
    for symbol,rows in enums.items():
        header+='enum class '+symbol+' : uint8_t {\n'+''.join('    '+r['symbol']+' = '+str(r['id'])+',\n' for r in rows)+'};\n'
    header+='struct EggGachaThresholds {uint16_t common,rare,epic,legendaryOffset;};\ninline constexpr EggGachaThresholds kEggGachaThresholds={'+','.join(str(value) for value in gacha_constants.values())+'};\n'
    header+='struct EggPityThresholds {uint32_t rare,epic,legendary;};\ninline constexpr EggPityThresholds kEggPityThresholds={'+','.join(str(value) for value in pity_constants.values())+'};\n'
    header+='struct EggVoucherOffer {uint8_t cursor;VoucherType voucher;uint16_t consumed,pulls;};\ninline constexpr EggVoucherOffer kEggVoucherOffers[]={\n'+''.join('    {'+str(r['cursor'])+',VoucherType::'+r['voucher']+','+str(r['consumed'])+','+str(r['pulls'])+'},\n' for r in offers)+'};\ninline constexpr uint32_t kEggGachaInventoryLimit='+str(inventory_limit)+';\n'
    header+='struct EggIncubationPolicy { EggTier tier; uint16_t waves; };\ninline constexpr EggIncubationPolicy kEggIncubationPolicies[]={\n'
    header+=''.join('    {EggTier::'+r['symbol']+','+str(constants['HATCH_WAVES_'+r['symbol']+'_EGG'])+'},\n' for r in enums['EggTier'])+'};\n'
    header+='inline constexpr uint16_t kSpecialEggIncubationSpecies[]={'+','.join(str(r['id']) for r in special_species)+'};\n'
    header+='inline constexpr uint16_t kManaphyEggHatchWaves='+str(constants['HATCH_WAVES_MANAPHY_EGG'])+';\n}\n'
    canonical_path=root/'project/data/pokerogue/canonical-content.json'
    canonical_raw=canonical_path.read_bytes()
    canonical=json.loads(canonical_raw)
    if canonical.get('sourceSnapshot',{}).get('sourceType')=='TEST_FIXTURE' or canonical.get('provenance',{}).get('sourceType')=='TEST_FIXTURE':
        raise ValueError('Production egg policy cannot use fixtures')
    if canonical.get('sourceSnapshot',{}).get('revision')!=GAME_REV:
        raise ValueError('Canonical egg data uses a different pinned revision')
    species_tiers=[]
    for species in canonical['collections']['species']:
        declaration=re.search(r'\beggTier\s*:\s*([^,}\n]+)',species['extensions']['upstreamRawRecord']['value'])
        expected=re.fullmatch(r'EggTier\.(COMMON|RARE|EPIC|LEGENDARY)',declaration[1].strip())[1] if declaration else 'COMMON'
        if species.get('eggTier')!=expected: raise ValueError('Canonical egg tier differs from pinned source: '+species['id'])
        species_tiers.append({'dex':species['nationalDexId'],'tier':expected,'declared':bool(declaration)})
    species_tiers.sort(key=lambda row:row['dex'])
    if len({r['dex'] for r in species_tiers})!=len(species_tiers): raise ValueError('Duplicate canonical species IDs')
    header=header[:-2]+'struct SpeciesEggTier {uint16_t dex; EggTier tier; bool declared;};\ninline constexpr SpeciesEggTier kSpeciesEggTiers[]={\n'
    header+=''.join('    {'+str(r['dex'])+',EggTier::'+r['tier']+','+('true' if r['declared'] else 'false')+'},\n' for r in species_tiers)+'};\n}\n'
    target=root/'project/generated/include/content/EggContentPolicy.hpp';target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(header.encode('utf-8'))
    report={'schemaVersion':1,'sources':[{'repository':REPOSITORY,'revision':GAME_REV,'sourcePath':p,'sourceSymbol':{'src/enums/egg-type.ts':'EggTier','src/enums/egg-source-types.ts':'EggSourceType','src/enums/voucher-type.ts':'VoucherType','src/data/balance/rates.ts':'HATCH_WAVES_*_EGG/GACHA_*_THRESHOLD/GACHA_LEGENDARY_UP_THRESHOLD_OFFSET/EGG_PITY_*_THRESHOLD','src/data/egg.ts':'Egg.getEggTierDefaultHatchWaves/rollEggTier/checkForPityTierOverrides','src/phases/egg-lapse-phase.ts':'EggLapsePhase.start','src/system/egg-data.ts':'EggData','src/enums/variant-tier.ts':'VariantTier','src/enums/species-id.ts':'SpeciesId','src/data/species-data-registry.ts':'SpeciesDataRegistry.getEggTier/getSpeciesForEggTier','src/ui/handlers/egg-gacha-ui-handler.ts':'EggGachaUiHandler.cursorToVoucher/handleVoucherSelectAction'}[p],'schemaVersion':1,'hash':hashlib.sha256(raw[p]).hexdigest()} for p in paths],'enums':enums,'incubationConstants':constants,'gachaThresholds':gacha_constants,'pityThresholds':pity_constants,'voucherOffers':offers,'gachaInventoryLimit':inventory_limit,'specialIncubationSpecies':special_species,'canonicalInput':{'sourcePath':'project/data/pokerogue/canonical-content.json','hash':hashlib.sha256(canonical_raw).hexdigest()},'speciesTiers':species_tiers,'scope':'IDENTIFIERS_SPECIES_TIERS_AND_INCUBATION_CONSTANTS','runtimeIntegration':'PENDING_INVENTORY_GACHA_HATCHING','generatedSHA256':hashlib.sha256(target.read_bytes()).hexdigest()}
    destination=root/'docs/generated/EGG_CONTENT_IMPORT_REPORT.json';destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes((json.dumps(report,ensure_ascii=False,sort_keys=True,indent=2)+'\n').encode('utf-8'))
    return report
if __name__=='__main__':
    print(prepare()['generatedSHA256'])
