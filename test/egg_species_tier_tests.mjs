import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
const content=JSON.parse(await fs.readFile(new URL('../project/data/pokerogue/canonical-content.json',import.meta.url),'utf8'));
const counts=new Map();
for(const species of content.collections.species) {
  const raw=species.extensions?.upstreamRawRecord?.value;
  assert.equal(typeof raw,'string',`${species.id}: missing inspected upstream record`);
  const declaration=raw.match(/\beggTier\s*:\s*([^,}\n]+)/);
  const expected=declaration ? /^EggTier\.(COMMON|RARE|EPIC|LEGENDARY)$/.exec(declaration[1].trim())?.[1] : 'COMMON';
  assert(expected,`${species.id}: unsupported upstream egg tier`);
  assert.equal(species.eggTier,expected,`${species.id}: canonical egg tier differs from upstream declaration`);
  assert(species.source?.sourcePath && species.source?.sourceSymbol && species.source?.sourceHash,`${species.id}: missing provenance`);
  counts.set(expected,(counts.get(expected)??0)+1);
}
assert.equal(counts.size,4,'Pinned species declarations must exercise all four egg tiers');
console.log('PASS real species egg tiers:',Object.fromEntries(counts));
