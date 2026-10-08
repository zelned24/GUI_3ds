import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';
import {fileURLToPath} from 'node:url';
import {PokerogueImporter,parseUpstreamGenderDifferences,parseGenderSpriteExclusions} from '../tools/js/data/PokerogueImporter.js';
import {SpeciesDefinition} from '../tools/js/data/CanonicalModels.js';
import {POKEROGUE_REPOSITORIES} from '../tools/js/data/PokerogueSource.js';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const read=source=>execFileSync('git',['show',POKEROGUE_REPOSITORIES.pokerogue.revision+':'+source],{cwd:path.join(root,'build/upstream/pokerogue'),encoding:'utf8'});
const importer=new PokerogueImporter();
importer.speciesEnumCatalog=importer.enumParser.parseEnum(read('src/enums/species-id.ts'),'SpeciesId','src/enums/species-id.ts');
const source=read('src/data/balance/species/generation-01.ts');
const rows=importer.parseSpeciesFromGeneration(source,['BULBASAUR','VENUSAUR','PIKACHU']);
assert.equal(rows.length,3);
assert.equal(rows.find(row=>row.id==='bulbasaur').genderDiffs,false);
assert.equal(rows.find(row=>row.id==='venusaur').genderDiffs,true);
assert.equal(rows.find(row=>row.id==='pikachu').genderDiffs,true);
for(const row of rows) assert(row.extensions.upstreamRawRecord.value.includes('genderDiffs'));
// Change only an isolated parser input; no upstream file is modified.
const changed=source.replace('genderDiffs: false','genderDiffs: unsupportedExpression');
assert.throws(()=>importer.parseSpeciesFromGeneration(changed,['BULBASAUR']),/unsupported genderDiffs/);
assert.equal(new SpeciesDefinition({id:'test-only'}).genderDiffs,null);
assert.throws(()=>new SpeciesDefinition({id:'test-only',genderDiffs:'false'}),/Invalid canonical/);
console.log('PASS pinned species gender metadata: true, false, unknown and invalid values');

const forms=JSON.parse(fs.readFileSync(path.join(root,'project/data/pokerogue/canonical-content.json'),'utf8')).collections.forms;
let yes=0,no=0,unknown=0;
for(const form of forms) {
  const raw=form.extensions.upstreamRawRecord.value;
  const value=parseUpstreamGenderDifferences(raw,form.id);
  const declared=/\bgenderDiffs\s*:/.test(raw);
  if(declared) {assert.equal(typeof value,'boolean');if(value) ++yes;else ++no;}
  else {assert.equal(value,null);++unknown;}
}
assert(yes>0 && no>0 && unknown>0);
assert.equal(parseUpstreamGenderDifferences('{}','test-only'),null);
assert.throws(()=>parseUpstreamGenderDifferences('{genderDiffs: inherited}','test-only'),/unsupported/);
console.log(`PASS preserved real form fragments: ${yes} gender differences, ${no} without differences, ${unknown} undeclared`);

const symbols=new Map([...read('src/enums/species-form-key.ts').matchAll(/([A-Z_]+)\s*=\s*"([^"]+)"/g)].map(match=>[match[1],match[2]]));
const excluded=parseGenderSpriteExclusions(read('src/data/pokemon-species.ts'),symbols);
assert(excluded.includes('mega-x') && excluded.includes('primal') && excluded.includes('gigantamax'));
assert(!excluded.includes('female'));
assert.throws(()=>parseGenderSpriteExclusions(read('src/data/pokemon-species.ts'),new Map()),/Unresolved/);
assert.throws(()=>parseGenderSpriteExclusions('',symbols),/Missing/);
console.log('PASS pinned gender sprite form exclusions: '+excluded.join(', '));
