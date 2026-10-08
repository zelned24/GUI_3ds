import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {buildPokemonAppearanceHeader} from '../scripts/generate_pokemon_appearance_index.mjs';
import {POKEROGUE_REPOSITORIES} from '../tools/js/data/PokerogueSource.js';
// Isolated parser tests: these bytes are never staged as production textures.
const sha=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
const metadata=Buffer.alloc(116),texture=Buffer.from('test texture');
metadata.write('P3ATLAS1');metadata.writeUInt32LE(1,8);metadata.writeUInt16LE(32,12);metadata.writeUInt16LE(32,14);metadata.writeUInt32LE(1,16);
const metadataPath='build/romfs/sprites/pokemon/atlas/front/1-shiny-v1.p3a';
const texturePath='build/romfs/sprites/pokemon/1-shiny-v1.t3x';
const files=new Map([[metadataPath,metadata],[texturePath,texture]]);
const row={atlasKey:'1-shiny-v1',facing:'front',metadataPath,metadataSha256:sha(metadata),
  textures:[{path:texturePath,sha256:sha(texture)}],
  appearance:{schemaVersion:1,atlasKey:'1',facing:'front',female:false,shiny:true,variant:1,
    upstreamGameRevision:POKEROGUE_REPOSITORIES.pokerogue.revision}};
const inventory={repository:POKEROGUE_REPOSITORIES['pokerogue-assets'].url,
  revision:POKEROGUE_REPOSITORIES['pokerogue-assets'].revision,assets:[row]};
const read=async relative=>{assert(files.has(relative));return files.get(relative);};
const build=value=>buildPokemonAppearanceHeader(Buffer.from(JSON.stringify(value)),read);
const result=await build(inventory);
assert.equal(result.count,1);
assert(result.header.includes('{"1",false,false,1,"1-shiny-v1",true}'));
assert.equal(result.header,(await build(inventory)).header);
const femaleMetadataPath='build/romfs/sprites/pokemon/atlas/front/1-female.p3a';
const femaleTexturePath='build/romfs/sprites/pokemon/1-female.t3x';
files.set(femaleMetadataPath,metadata);files.set(femaleTexturePath,texture);
const normalFemale={...row,atlasKey:'1-female',metadataPath:femaleMetadataPath,
  textures:[{path:femaleTexturePath,sha256:sha(texture)}],
  appearance:{...row.appearance,female:true,shiny:false,variant:0}};
const mixed=await build({...inventory,assets:[row,normalFemale]});
assert.equal(mixed.count,2);
// The binary lookup relies on this exact tuple order, including normal versus shiny.
const order=mixed.header.indexOf('{"1",false,false,1,"1-shiny-v1",true}');
assert(order>=0 && order<mixed.header.indexOf('{"1",false,true,0,"1-female",false}'));
assert(mixed.header.includes('int(row.shiny)-int(shiny)'));
const otherMetadataPath='build/romfs/sprites/pokemon/atlas/front/1-female-shiny-v0.p3a';
const otherTexturePath='build/romfs/sprites/pokemon/1-female-shiny-v0.t3x';
files.set(otherMetadataPath,metadata);files.set(otherTexturePath,texture);
const femaleShiny={...normalFemale,atlasKey:'1-female-shiny-v0',metadataPath:otherMetadataPath,
  textures:[{path:otherTexturePath,sha256:sha(texture)}],appearance:{...normalFemale.appearance,shiny:true}};
const ordered=await build({...inventory,assets:[femaleShiny,normalFemale,row]});
assert.equal(ordered.count,3);
assert(ordered.header.indexOf('{"1",false,true,0,"1-female",false}')<
  ordered.header.indexOf('{"1",false,true,0,"1-female-shiny-v0",true}'));

assert(mixed.header.includes('{"1",false,true,0,"1-female",false}'));
await assert.rejects(()=>build({...inventory,assets:[{...normalFemale,
  appearance:{...normalFemale.appearance,variant:1}}]}),/identity/);
for(const patch of [
  {atlasKey:'1-shiny-v2'}, {metadataPath:'../invalid.p3a'},
  {metadataPath:metadataPath.replace('1-shiny','2-shiny')}, {metadataSha256:'0'.repeat(64)},
  {textures:[]}, {textures:[{path:texturePath.replace('1-shiny','2-shiny'),sha256:sha(texture)}]}, {textures:[{path:texturePath,sha256:'0'.repeat(64)}]},
  {appearance:{...row.appearance,variant:3}},
  {appearance:{...row.appearance,female:true}},
  {appearance:{...row.appearance,upstreamGameRevision:'0'.repeat(40)}},
]) await assert.rejects(()=>build({...inventory,assets:[{...row,...patch}]}));
await assert.rejects(()=>build({...inventory,assets:[row,row]}),/Duplicate/);
await assert.rejects(()=>build({...inventory,revision:'0'.repeat(40)}),/pinned/);
// Valid paged metadata resolves the renderer's -pN paths, never the base sheet.
const paged=Buffer.from(metadata);paged.write('P3ATLAS2');paged.writeUInt32LE(2,8);
const pagedPath=texturePath.replace('.t3x','-p0.t3x');
files.set(metadataPath,paged);files.set(pagedPath,texture);
const pagedRow={...row,metadataSha256:sha(paged),textures:[{path:pagedPath,sha256:sha(texture)}]};
assert.equal((await build({...inventory,assets:[pagedRow]})).count,1);
await assert.rejects(()=>build({...inventory,assets:[{...pagedRow,textures:row.textures}]}),/path/);
const oversized=Buffer.from(paged);oversized.writeUInt16LE(8,114);files.set(metadataPath,oversized);
await assert.rejects(()=>build({...inventory,assets:[{...pagedRow,metadataSha256:sha(oversized)}]}),/capacity/);
const truncated=metadata.subarray(0,90);files.set(metadataPath,truncated);
await assert.rejects(()=>build({...inventory,assets:[{...row,metadataSha256:sha(truncated)}]}),/metadata/);
files.set(metadataPath,metadata);
const empty=await build({...inventory,assets:[]});
assert.equal(empty.count,0);assert(empty.header.includes('std::array<PokemonAppearanceAsset, 0>'));
console.log('PASS appearance index: deterministic output, identity, pins, paths, corruption, duplicates and empty catalog');
