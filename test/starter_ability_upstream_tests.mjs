import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {POKEROGUE_REPOSITORIES} from '../tools/js/data/PokerogueSource.js';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const read=file=>execFileSync('git',['show',POKEROGUE_REPOSITORIES.pokerogue.revision+':'+file],
  {cwd:path.join(root,'build/upstream/pokerogue'),encoding:'utf8'});
const attributes=read('src/enums/ability-attr.ts').match(/Object\.freeze\(\{([\s\S]*?)\}\)/);
assert(attributes);
assert(/^[\sA-Z_0-9:,]+$/.test(attributes[1]));
const AbilityAttr=JSON.parse('{'+attributes[1].replace(/([A-Z_][A-Z_0-9]*)\s*:/g,'"$1":').replace(/,\s*$/,'')+'}');
const speciesSource=read('src/data/pokemon-species.ts');
assert(speciesSource.includes('this.ability2 = data.ability2 === AbilityId.NONE ? data.ability1 : data.ability2;'));
const functionSource=read('src/ui/utils/starter-select-ui-utils.ts');
const match=functionSource.match(/function getStarterDefaultAbilityIndex\(starterId: StarterSpeciesId\): number \{([\s\S]*?)\n\}/);
assert(match);
// Execute this one inspected pure upstream function in a test only. It is never bundled in runtime.
let currentSpecies,currentMask;
const oracle=new Function('getStarterData','speciesDataRegistry','AbilityAttr',
  'return function(starterId) {'+match[1]+'\n};')(
  ()=>({starterDataEntry:{abilityAttr:currentMask}}),{getSpecies:()=>currentSpecies},AbilityAttr);
const species=JSON.parse(fs.readFileSync(path.join(root,'project/data/pokerogue/canonical-content.json'),'utf8')).collections.species;
let cases=0,singleRegular=0;
for(const row of species) {
  currentSpecies={ability1:row.abilities.ability1,
    ability2:row.abilities.ability2==='NONE' ? row.abilities.ability1 : row.abilities.ability2};
  if(row.abilities.ability2==='NONE') ++singleRegular;
  for(currentMask=1;currentMask<8;++currentMask) {
    const expected=currentMask & AbilityAttr.ABILITY_1 ? 0 : currentMask & AbilityAttr.ABILITY_2 ? 1 : 2;
    assert.equal(oracle(row.id),expected,row.id+':'+currentMask);++cases;
  }
}
assert(singleRegular>0);
console.log(`PASS pinned upstream ability-slot oracle: ${cases} cases; ${singleRegular} raw NONE second abilities. C++ execution pending.`);
