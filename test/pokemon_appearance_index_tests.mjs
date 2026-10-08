import assert from 'node:assert/strict';
import crypto from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {buildPokemonAppearanceHeader,validateMaterializedAppearanceCatalog,materializedAppearanceCatalogHash} from '../scripts/generate_pokemon_appearance_index.mjs';
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

const complete={schemaVersion:1,repository:inventory.repository,revision:inventory.revision,
  requested:1,materialized:[{atlasKey:'1',facing:'front',female:false,variant:1,pngSHA256:sha(texture)}],
  missingInUpstream:[],unsupported:[],contentHash:sha(texture)};
complete.contentHash=materializedAppearanceCatalogHash(complete);
assert.equal(validateMaterializedAppearanceCatalog(complete).size,1);
for(const patch of [{requested:2},{contentHash:undefined},{unsupported:[{}]},{revision:'0'.repeat(40)},
  {requested:2,materialized:[...complete.materialized,...complete.materialized]}])
  assert.throws(()=>validateMaterializedAppearanceCatalog({...complete,...patch}));

assert.equal(validateMaterializedAppearanceCatalog(complete).get("1:front:false:1"),sha(texture));

assert.throws(()=>validateMaterializedAppearanceCatalog({...complete,contentHash:"0".repeat(64)}),/hash/);

// Cross-language proof of the report format actually written by the Python materializer.
const unicodeReport={...complete,note:'Pokémon 漢字 😀',nested:{z:2,a:true}};
const pythonHash=execFileSync('python',['-c',
  'import sys,json,hashlib; d=json.loads(sys.stdin.buffer.read().decode("utf-8")); d.pop("contentHash",None); print(hashlib.sha256(json.dumps(d,sort_keys=True,separators=(",",":")).encode()).hexdigest())'],
  {input:JSON.stringify(unicodeReport),encoding:'utf8'}).trim();
assert.equal(materializedAppearanceCatalogHash(unicodeReport),pythonHash);

const seal=report=>({...report,contentHash:materializedAppearanceCatalogHash(report)});
const missingRow={atlasKey:'2',facing:'back',female:true,variant:0,
  classification:'MISSING_IN_PINNED_UPSTREAM',source:inventory.revision+':images/pokemon/back/shiny/female/2.json'};
const withMissing={...complete,requested:2,missingInUpstream:[missingRow]};
assert.equal(validateMaterializedAppearanceCatalog(seal(withMissing)).size,1);
for(const patch of [{classification:'INVALID_IMPORT'}, {source:'other:images/pokemon/2.json'},
  {source:inventory.revision+':images/pokemon/../2.json'}, {female:'true'}, {variant:3},
  {atlasKey:'1',facing:'front',female:false,variant:1}])
  assert.throws(()=>validateMaterializedAppearanceCatalog(seal({...withMissing,missingInUpstream:[{...missingRow,...patch}]})));
assert.throws(()=>validateMaterializedAppearanceCatalog(seal({...complete,requested:2,
  materialized:[...complete.materialized,...complete.materialized]})),/Duplicate/);
assert.throws(()=>validateMaterializedAppearanceCatalog(seal({...withMissing,requested:3,
  missingInUpstream:[missingRow,missingRow]})),/duplicate/);
console.log('PASS completed catalog: Python-compatible hash, missing classification and identity conflicts');

assert.equal(validateMaterializedAppearanceCatalog(complete,new Set(['1:front:false:1'])).size,1);
assert.throws(()=>validateMaterializedAppearanceCatalog(complete,new Set(['2:front:false:1'])),/omits/);
assert.throws(()=>validateMaterializedAppearanceCatalog(complete,new Set(['1:front:false:1','2:front:false:1'])),/enumeration/);
assert.equal(validateMaterializedAppearanceCatalog(seal(withMissing),new Set(['1:front:false:1','2:back:true:0'])).size,1);
