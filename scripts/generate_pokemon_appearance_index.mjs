import fs from 'node:fs/promises';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {POKEROGUE_REPOSITORIES} from '../tools/js/data/PokerogueSource.js';

// Only physically converted, hash-checked textures enter the runtime index.
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
// Matches Python json.dumps(sort_keys=True, separators=(",", ":")) for this
// integer/string/boolean report schema, including ensure_ascii Unicode escaping.
export function materializedAppearanceCatalogHash(catalog) {
  const ordered=value=>Array.isArray(value) ? value.map(ordered) : value && typeof value==='object'
    ? Object.fromEntries(Object.keys(value).sort().map(key=>[key,ordered(value[key])])) : value;
  const {contentHash,...payload}=catalog;
  const bytes=JSON.stringify(ordered(payload)).replace(/[\u007f-\uffff]/g,
    character=>'\\u'+character.charCodeAt(0).toString(16).padStart(4,'0'));
  return crypto.createHash('sha256').update(bytes).digest('hex');
}
export function validateMaterializedAppearanceCatalog(catalog) {
  const pinned=POKEROGUE_REPOSITORIES['pokerogue-assets'];
  if(catalog?.schemaVersion!==1 || catalog.repository!==pinned.url || catalog.revision!==pinned.revision ||
    !Number.isInteger(catalog.requested) || catalog.requested<1 || !Array.isArray(catalog.materialized) ||
    !Array.isArray(catalog.missingInUpstream) || !Array.isArray(catalog.unsupported) || catalog.unsupported.length ||
    catalog.materialized.length+catalog.missingInUpstream.length!==catalog.requested ||
    !/^[0-9a-f]{64}$/.test(catalog.contentHash??'')) throw new Error('Incomplete or invalid pinned appearance catalog');
  if(materializedAppearanceCatalogHash(catalog)!==catalog.contentHash)
    throw new Error('Materialized appearance catalog hash mismatch');
  const identities=new Map();
  for(const row of catalog.materialized) {
    if(!/^[1-9][0-9]*(?:-[a-z0-9-]+)?$/.test(row.atlasKey) || !['front','back'].includes(row.facing) ||
      typeof row.female!=='boolean' || ![0,1,2].includes(row.variant) || !/^[0-9a-f]{64}$/.test(row.pngSHA256))
      throw new Error('Invalid materialized appearance record');
    const identity=[row.atlasKey,row.facing,row.female,row.variant].join(':');
    if(identities.has(identity)) throw new Error('Duplicate materialized appearance record');
    identities.set(identity,row.pngSHA256);
  }
  const missing=new Set();
  for(const row of catalog.missingInUpstream) {
    if(!/^[1-9][0-9]*(?:-[a-z0-9-]+)?$/.test(row.atlasKey) || !['front','back'].includes(row.facing) ||
      typeof row.female!=='boolean' || ![0,1,2].includes(row.variant) ||
      row.classification!=='MISSING_IN_PINNED_UPSTREAM' || typeof row.source!=='string' ||
      !row.source.startsWith(pinned.revision+':images/pokemon/') ||
      row.source.split(':').length!==2 || row.source.split('/').some(part=>part==='.' || part==='..') ||
      row.source.includes('\\')) throw new Error('Invalid missing upstream appearance record');
    const identity=[row.atlasKey,row.facing,row.female,row.variant].join(':');
    if(identities.has(identity) || missing.has(identity)) throw new Error('Conflicting or duplicate missing appearance');
    missing.add(identity);
  }
  return identities;
}
export async function buildPokemonAppearanceHeader(raw,readPhysical=relative=>fs.readFile(path.resolve(root,relative))) {
const inventory=JSON.parse(raw);
const pinned=POKEROGUE_REPOSITORIES['pokerogue-assets'];
if(inventory.repository!==pinned.url || inventory.revision!==pinned.revision || !Array.isArray(inventory.assets))
  throw new Error('Appearance index source does not match pinned assets');
const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
async function checked(relative,expected) {
  if(typeof relative!=='string' || path.isAbsolute(relative) || relative.split(/[\\/]/).some(p=>p==='.' || p==='..'))
    throw new Error('Unsafe converted appearance path');
  const physical=path.resolve(root,relative);
  if(!physical.startsWith(root+path.sep)) throw new Error('Unsafe converted appearance path');
  const bytes=await readPhysical(relative);
  if(!/^[0-9a-f]{64}$/.test(expected) || sha(bytes)!==expected)
    throw new Error('Converted appearance file hash mismatch');
  return bytes;
}
const rows=[];
const identities=new Set();
for(const asset of inventory.assets) {
  const appearance=asset.appearance;
  if(!appearance) continue;
  if(appearance.schemaVersion!==1 || appearance.upstreamGameRevision!==POKEROGUE_REPOSITORIES.pokerogue.revision ||
     !/^[1-9][0-9]*(?:-[a-z0-9-]+)?$/.test(appearance.atlasKey) || typeof appearance.shiny!=='boolean' || (!appearance.shiny && (!appearance.female || appearance.variant!==0)) ||
     typeof appearance.female!=='boolean' || ![0,1,2].includes(appearance.variant) ||
     !['front','back'].includes(asset.facing) || appearance.facing!==asset.facing ||
     asset.atlasKey!==appearance.atlasKey+(appearance.female?'-female':'')+(appearance.shiny?'-shiny-v'+appearance.variant:''))
    throw new Error('Converted appearance identity mismatch');
  const identity=[appearance.atlasKey,asset.facing,appearance.female,appearance.shiny,appearance.variant].join(':');
  if(identities.has(identity)) throw new Error('Duplicate converted appearance');
  identities.add(identity);
  if(asset.metadataPath!==`build/romfs/sprites/pokemon/atlas/${asset.facing}/${asset.atlasKey}.p3a`)
    throw new Error('Appearance metadata path differs from runtime identity');
  const metadata=await checked(asset.metadataPath,asset.metadataSha256);
  const paged=metadata.subarray(0,8).toString()==='P3ATLAS2';
  if(metadata.length<84 || (!paged && metadata.subarray(0,8).toString()!=='P3ATLAS1') ||
     metadata.readUInt32LE(8)!==(paged?2:1) || !metadata.readUInt16LE(12) || !metadata.readUInt16LE(14) ||
     !metadata.readUInt32LE(16) || metadata.readUInt32LE(16)>1024 ||
     metadata.length!==84+metadata.readUInt32LE(16)*32)
    throw new Error('Invalid converted appearance metadata');
  const required=new Set();
  for(let offset=84;offset<metadata.length;offset+=32) {
    const flags=metadata.readUInt16LE(offset+30);
    if(!paged && flags>1) throw new Error('Invalid non-paged appearance flags');
    const page=paged?flags>>1:0;
    if(page>=4) throw new Error('Appearance exceeds runtime page capacity');
    required.add(page);
  }
  if(!Array.isArray(asset.textures) || !asset.textures.length) throw new Error('Appearance has no texture pages');
  if(asset.textures.length!==required.size) throw new Error('Appearance texture page count differs from metadata');
  for(let page=0;page<asset.textures.length;++page) {
    const texture=asset.textures[page];
    const expected=`build/romfs/sprites/pokemon/${asset.facing==='back'?'back/':''}${asset.atlasKey}${paged?'-p'+page:''}.t3x`;
    if(!required.has(page) || texture.path!==expected) throw new Error('Appearance texture path differs from runtime identity');
    await checked(texture.path,texture.sha256);
  }
  rows.push({baseKey:appearance.atlasKey,back:asset.facing==='back',female:appearance.female,
    variant:appearance.variant,shiny:appearance.shiny,atlasKey:asset.atlasKey});
}
rows.sort((a,b)=>a.baseKey<b.baseKey?-1:a.baseKey>b.baseKey?1:Number(a.back)-Number(b.back)||Number(a.female)-Number(b.female)||Number(a.shiny)-Number(b.shiny)||a.variant-b.variant);
const header=`// Generated from converted sprite inventory SHA-256 ${sha(raw)}.
// Assets repository: ${pinned.url} @ ${pinned.revision}
#pragma once
#include <array>
#include <cstring>
namespace Pokerogue3DS {
struct PokemonAppearanceAsset {const char* baseKey; bool back; bool female; unsigned variant; const char* atlasKey; bool shiny;};
inline constexpr std::array<PokemonAppearanceAsset, ${rows.length}> kPokemonAppearanceAssets{{
${rows.map(row=>`    {"${row.baseKey}",${row.back},${row.female},${row.variant},"${row.atlasKey}",${row.shiny}},`).join('\n')}
}};
inline const PokemonAppearanceAsset* findPokemonAppearanceAsset(const char* baseKey,bool back,bool female,unsigned variant,bool shiny=true) {
    if(!baseKey || variant>2) return nullptr;
    std::size_t first=0, last=kPokemonAppearanceAssets.size();
    while(first<last) {
        const auto middle=first+(last-first)/2;
        const auto& row=kPokemonAppearanceAssets[middle];
        int comparison=std::strcmp(row.baseKey,baseKey);
        if(!comparison) comparison=int(row.back)-int(back);
        if(!comparison) comparison=int(row.female)-int(female);
        if(!comparison) comparison=int(row.shiny)-int(shiny);
        if(!comparison) comparison=int(row.variant)-int(variant);
        if(comparison<0) first=middle+1;
        else last=middle;
    }
    if(first==kPokemonAppearanceAssets.size()) return nullptr;
    const auto& row=kPokemonAppearanceAssets[first];
    return row.back==back && row.female==female && row.shiny==shiny && row.variant==variant &&
        !std::strcmp(row.baseKey,baseKey) ? &row : nullptr;
}
}
`;
return {header,count:rows.length};
}
if(process.argv[1] && path.resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  const raw=await fs.readFile(path.join(root,'build/upstream-assets/converted-sprite-assets.json'));
  const result=await buildPokemonAppearanceHeader(raw);
  await fs.writeFile(path.join(root,'project/generated/include/content/PokemonAppearanceAssets.hpp'),result.header);
  console.log(`${result.count} converted Pokemon appearances indexed`);
}
