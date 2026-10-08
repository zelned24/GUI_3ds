import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import {parseUpstreamPassives} from '../tools/js/data/PokerogueImporter.js';
// Isolated malformed records are parser fixtures, never production content.
assert.equal(parseUpstreamPassives('{species: X}','fixture'),null);
assert.deepEqual(parseUpstreamPassives('{passives: AbilityId.GRASSY_SURGE,}','fixture').sharedSymbol,'GRASSY_SURGE');
assert.deepEqual(parseUpstreamPassives('{passives: {0: AbilityId.GRASSY_SURGE,1: AbilityId.SEED_SOWER,},}','fixture').byFormIndex,
  {0:'GRASSY_SURGE',1:'SEED_SOWER'});
for(const raw of ['{passives: callback(),}','{passives: {1: AbilityId.GRASSY_SURGE},}',
  '{passives: {0: AbilityId.NONE,0: AbilityId.STENCH},}','{passives: {0: unknown()},}'])
  assert.throws(()=>parseUpstreamPassives(raw,'fixture'),/Invalid import/);
const content=JSON.parse(await fs.readFile(new URL('../project/data/pokerogue/canonical-content.json',import.meta.url),'utf8'));
const abilities=new Set(content.collections.abilities.map(row=>row.abilityId));
let shared=0,byForm=0;
for(const species of content.collections.species) {
  const expected=parseUpstreamPassives(species.extensions.upstreamRawRecord.value,species.id);
  if(!expected) continue;
  const actual=species.extensions.upstreamPassives;
  assert(actual,`Missing normalized passive for ${species.id}`);
  assert.equal(actual.kind,expected.kind);
  assert.equal(actual.sharedSymbol,expected.sharedSymbol);
  assert.deepEqual(actual.byFormIndex,expected.byFormIndex);
  assert.equal(actual.source.revision,'8555c08c823b856cbec4eb99ca84ea52a955836d');
  assert.match(actual.source.sha256,/^[0-9a-f]{64}$/);
  if(actual.kind==='SHARED') {assert(abilities.has(actual.sharedId));++shared;}
  else {for(const id of Object.values(actual.byFormIndexIds)) assert(abilities.has(id));++byForm;}
}
assert(shared>0 && byForm>0);
console.log(`PASS real passive import: ${shared} shared species, ${byForm} form maps; IDs/provenance and malformed declarations`);
