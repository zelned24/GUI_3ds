import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const dataPath = path.join(root, 'project/data/pokerogue/canonical-content.json');
const reportPath = path.join(root, 'project/data/pokerogue/import-report.json');
const outputPath = path.join(root, 'project/generated/include/content/PokerogueRuntimeContent.hpp');
const content = JSON.parse(await fs.readFile(dataPath, 'utf8'));
const report = JSON.parse(await fs.readFile(reportPath, 'utf8'));
if (content.provenance?.sourceType === 'TEST_FIXTURE' || content.sourceSnapshot?.sourceType === 'TEST_FIXTURE') {
  throw new Error('Refusing to generate production 3DS content from TEST_FIXTURE');
}
if (!report.contentHash || !report.sourceRevisions?.pokerogue || content.collections?.species?.length === 0) {
  throw new Error('Import report is missing pinned production provenance');
}

const field = value => String(value ?? '').replaceAll('\\', '\\\\').replaceAll('"', '\\"').replaceAll('\r', '\\r').replaceAll('\n', '\\n').replaceAll('\t', '\\t');
const entityRows = records => records.map(item => `    {"${field(item.id)}", "${field(item.name ?? item.names?.en ?? item.extensions?.localeValue ?? item.value?.name ?? (typeof item.value === 'string' ? item.value : ''))}", "${field(item.provenance?.sourcePath ?? item.source?.sourcePath ?? item.metadata?.sourcePath ?? '')}", "${field(item.provenance?.sourceSymbol ?? item.source?.sourceSymbol ?? item.metadata?.sourceSymbol ?? '')}", "${field(item.provenance?.sourceHash ?? item.source?.sourceHash ?? item.metadata?.sourceHash ?? '')}"}`).join(',\n');
const collections = content.collections;
const speciesIds = new Set(collections.species.map(item => item.id));
const movesById = new Map(collections.moves.map(move => [move.id, move]));
const abilitiesByName = new Map(collections.abilities.flatMap(ability => [
  [String(ability.name ?? '').toLowerCase(), ability.abilityId],
  [String(ability.names?.en ?? '').toLowerCase(), ability.abilityId],
]).filter(([name]) => name));
const abilitiesBySymbol = new Map(collections.abilities.map(ability => [ability.source?.sourceSymbol, ability.abilityId]));
const abilityIdFor = value => {
  if (!value || String(value).toLowerCase() === 'none') return 0;
  const numericId = abilitiesByName.get(String(value).toLowerCase());
  if (!Number.isInteger(numericId) || numericId < 1) throw new Error(`Invalid canonical species ability reference: ${value}`);
  return numericId;
};
const formAbilityId = (form, fieldName) => {
  const raw = form.extensions?.upstreamRawRecord?.value ?? '';
  const symbol = raw.match(new RegExp(`${fieldName}\\s*:\\s*AbilityId\\.([A-Z0-9_]+)`))?.[1];
  if (!symbol || symbol === 'NONE') return 0;
  const id = abilitiesBySymbol.get(`AbilityId.${symbol}`);
  if (!Number.isInteger(id) || id < 1) throw new Error(`Invalid canonical form ability reference: ${form.id}.${fieldName}=${symbol}`);
  return id;
};
const levelMoves = [];
const learnsetRanges = new Map();
for (const item of collections.species) {
  const offset = levelMoves.length;
  for (const learned of item.levelMoves ?? []) {
    const move = movesById.get(learned.move ?? learned.id);
    if (!move || !Number.isInteger(move.moveId) || move.moveId < 1) {
      throw new Error(`Invalid canonical learnset reference: ${item.id} -> ${learned.move ?? learned.id}`);
    }
    // PokéRogue uses 0 for EVOLVE_MOVE and -1 for RELEARN_MOVE.
    if (!Number.isInteger(learned.level) || learned.level < -1 || learned.level > 127) {
      throw new Error(`Invalid canonical learnset level: ${item.id} -> ${move.id}`);
    }
    levelMoves.push({ dex: item.nationalDexId, level: learned.level, moveId: move.moveId });
  }
  learnsetRanges.set(item.id, { offset, count: levelMoves.length - offset });
  if (levelMoves.length - offset > 0xFFFF) throw new Error(`Canonical learnset for ${item.id} exceeds 16-bit count range`);
}
if (levelMoves.length > 0xFFFFFFFF) throw new Error('Canonical learnset table exceeds 32-bit offset range');
const evolutionEdges = collections.species.flatMap(species => (species.evolutions ?? []).map((edge, order) => {
  if (!edge.targetSpeciesId || !speciesIds.has(edge.targetSpeciesId)) {
    throw new Error(`Invalid canonical evolution link: ${species.id} -> ${edge.targetSpeciesId ?? '(missing)'}`);
  }
  if (!Number.isFinite(edge.level)) throw new Error(`Invalid canonical evolution level: ${species.id} -> ${edge.targetSpeciesId}`);
  return { species, edge, order };
}));
const evolutionRows = evolutionEdges.map(({ species, edge, order }) => `    {"${field(species.id)}", "${field(edge.targetSpeciesId)}", ${edge.level}, ${edge.evoLevelThreshold?.strong ?? -1}, ${edge.evoLevelThreshold?.normal ?? -1}, ${edge.evoLevelThreshold?.wild ?? -1}, ${order}, "${field(edge.source?.sourcePath ?? species.source?.sourcePath ?? '')}", "${field(edge.source?.sourceSymbol ?? '')}", "${field(edge.source?.sourceHash ?? species.source?.sourceHash ?? '')}"}`).join(',\n');
const speciesRows = collections.species.map(item => {
  const asset = collections.assetReferences.find(entry => entry.speciesId === item.nationalDexId) ?? item.extensions?.assetReference;
  const form = collections.forms.find(entry => entry.speciesId === item.id);
  const stats = item.baseStats;
  for (const key of ['hp', 'atk', 'def', 'spatk', 'spdef', 'spd']) {
    if (!Number.isInteger(stats?.[key]) || stats[key] < 0 || stats[key] > 255) throw new Error(`Invalid canonical base stat ${item.id}.${key}`);
  }
  const learnset = learnsetRanges.get(item.id);
  const abilityData = item.abilities ?? {};
  const primaryAbilityId = abilityIdFor(abilityData.primary ?? abilityData.ability1);
  const secondaryAbilityId = abilityIdFor(abilityData.secondary ?? abilityData.ability2) || primaryAbilityId;
  const flag = value => value === true ? 1 : value === false ? 0 : -1;
  const malePercentTenths = item.malePercent === null ? 0xFFFE
    : Number.isFinite(item.malePercent) ? Math.round(item.malePercent * 10) : 0xFFFF;
  if (malePercentTenths !== 0xFFFE && malePercentTenths !== 0xFFFF && (malePercentTenths < 0 || malePercentTenths > 1000)) {
    throw new Error(`Canonical malePercent exceeds compact runtime range: ${item.id}`);
  }
  return `    {${item.nationalDexId}, ${malePercentTenths}, ${item.generation ?? 0}, ${item.starterCost ?? -1}, ${item.starterEligible === true ? 'true' : 'false'}, ${item.baseTotal ?? 0}, ${stats.hp}, ${stats.atk}, ${stats.def}, ${stats.spatk}, ${stats.spdef}, ${stats.spd}, ${primaryAbilityId}, ${secondaryAbilityId}, ${abilityIdFor(abilityData.hidden ?? abilityData.abilityHidden)}, ${abilityIdFor(abilityData.passive)}, ${learnset.offset}, ${learnset.count}, ${flag(item.rarity?.legendary)}, ${flag(item.rarity?.subLegendary)}, ${flag(item.rarity?.mythical)}, "${field(item.growthRate ?? '')}", "${field(item.id)}", "${field(item.name)}", "${field(item.type1)}", "${field(item.type2)}", "${field(form?.id ?? '')}", "${field(asset?.sourcePath ?? '')}", "${field(item.source?.sourcePath ?? '')}", "${field(item.source?.sourceSymbol ?? '')}", "${field(item.source?.sourceHash ?? '')}"}`;
}).join(',\n');
const localeRows = collections.locales
  .filter(item => ['pokemon', 'move', 'ability', 'item', 'gameMode', 'biomes'].includes(item.namespace))
  .map(item => ({ id: `${item.locale}:${item.namespace}:${item.canonicalId}`, name: item.value?.name ?? (typeof item.value === 'string' ? item.value : ''), provenance: item.source }));
const locales = entityRows(localeRows);
const modes = entityRows(collections.gameModes);
const biomes = entityRows(collections.biomes.map(item => ({
  ...item,
  name: collections.locales.find(entry => entry.namespace === 'biomes' && entry.canonicalId === item.id && entry.locale === 'en')?.value?.name ?? item.id,
})));
const biomeEncounterPools = collections.biomes.flatMap(biome => Object.entries(biome.encounterPools ?? {}).flatMap(([tier, times]) => Object.entries(times).flatMap(([time, speciesIds]) => speciesIds.map((speciesId, memberIndex) => ({ biome, tier, time, speciesId, memberIndex }))))).sort((a, b) => a.biome.id.localeCompare(b.biome.id) || a.tier.localeCompare(b.tier) || a.time.localeCompare(b.time));
const encounterPoolRows = biomeEncounterPools.map(({ biome, tier, time, speciesId, memberIndex }) => `    {"${field(biome.id)}", "${field(tier)}", "${field(time)}", ${memberIndex}, "${field(speciesId)}", "${field(biome.provenance.sourcePath)}", "${field(biome.provenance.sourceSymbol)}.pokemonPool.${field(tier)}.${field(time)}", "${field(biome.provenance.sourceHash)}"}`).join(',\n');
const biomeTrainerPools = collections.biomes.flatMap(biome => Object.entries(biome.trainerPools ?? {}).flatMap(([tier, trainerIds]) => trainerIds.map(trainerId => ({ biome, tier, trainerId })))).sort((a, b) => a.biome.id.localeCompare(b.biome.id) || a.tier.localeCompare(b.tier) || a.trainerId.localeCompare(b.trainerId));
const trainerPoolRows = biomeTrainerPools.map(({ biome, tier, trainerId }) => `    {"${field(biome.id)}", "${field(tier)}", "${field(trainerId)}", "${field(biome.provenance.sourcePath)}", "${field(biome.provenance.sourceSymbol)}.trainerPool.${field(tier)}", "${field(biome.provenance.sourceHash)}"}`).join(',\n');
const routes = entityRows(collections.routes);
const speciesById = new Map(collections.species.map(item => [item.id, item]));
const forms = collections.forms.map(form => {
  if (!speciesById.has(form.speciesId)) throw new Error(`Canonical form references missing species: ${form.id} -> ${form.speciesId}`);
  const stats = form.baseStats;
  for (const key of ['hp', 'atk', 'def', 'spatk', 'spdef', 'spd']) {
    if (!Number.isInteger(stats?.[key]) || stats[key] < 0 || stats[key] > 255) throw new Error(`Invalid canonical form base stat ${form.id}.${key}`);
  }
  if (!Array.isArray(form.types) || form.types.length < 1 || form.types.length > 2) throw new Error(`Invalid canonical form types: ${form.id}`);
  const formPrimaryAbility = formAbilityId(form, 'ability1');
  const abilities = [formPrimaryAbility, formAbilityId(form, 'ability2') || formPrimaryAbility,
    formAbilityId(form, 'abilityHidden')];
  return `    {"${field(form.id)}", "${field(form.speciesId)}", "${field(form.formKey)}", "${field(form.name)}", ${stats.hp}, ${stats.atk}, ${stats.def}, ${stats.spatk}, ${stats.spdef}, ${stats.spd}, ${abilities[0]}, ${abilities[1]}, ${abilities[2]}, "${field(form.types[0])}", "${field(form.types[1] ?? '')}", "${field(form.provenance?.sourcePath ?? '')}", "${field(form.provenance?.sourceSymbol ?? '')}", "${field(form.provenance?.sourceHash ?? '')}"}`;
}).join(',\n');
const moves = collections.moves.map(move => {
  const category = { Physical: 0, Special: 1, Status: 2 }[move.category];
  if (!Number.isInteger(move.moveId) || move.moveId < 1 || move.moveId > 0xFFFF || !Number.isInteger(category)) throw new Error(`Invalid canonical move record: ${move.id}`);
  for (const key of ['power', 'accuracy', 'pp', 'priority']) {
    if (!Number.isInteger(move[key])) throw new Error(`Invalid canonical move field: ${move.id}.${key}`);
  }
  if (move.power < -32768 || move.power > 32767 || move.accuracy < -32768 || move.accuracy > 32767 || move.pp < -32768 || move.pp > 32767 || move.priority < -128 || move.priority > 127 || (move.extensions?.upstreamGeneration ?? 0) > 255) {
    throw new Error(`Canonical move field exceeds compact native representation: ${move.id}`);
  }
  const upstreamRaw = move.extensions?.upstreamEffectMetadata?.value ?? '';
  const upstreamFlags = [];
  if (move.isUnimplemented || /\.unimplemented\s*\(/.test(upstreamRaw)) upstreamFlags.push('MoveIsUnimplemented');
  if (/SacrificialAttrOnHit/.test(upstreamRaw)) upstreamFlags.push('MoveHasSacrificialAttrOnHit');
  return `    {${move.moveId}, ${category}, ${move.power}, ${move.accuracy}, ${move.pp}, ${move.priority}, ${move.extensions?.upstreamChance ?? 0}, ${move.extensions?.upstreamGeneration ?? 0}, ${upstreamFlags.join(' | ') || 0}, "${field(move.id)}", "${field(move.names?.en ?? move.name)}", "${field(move.type)}", "${field(move.target ?? '')}", "${field(move.source?.sourcePath ?? '')}", "${field(move.source?.sourceSymbol ?? '')}", "${field(move.source?.sourceHash ?? '')}"}`;
}).join(',\n');
const abilities = entityRows(collections.abilities);
const items = entityRows(collections.items);

const header = `// Generated from pinned canonical PokéRogue content. Do not edit by hand.\n#pragma once\n#include <cstddef>\n#include <cstdint>\nnamespace PokerogueContent {\ninline constexpr char kContentHash[] = "${report.contentHash}";\ninline constexpr char kPokerogueRevision[] = "${report.sourceRevisions?.pokerogue ?? ''}";\ninline constexpr char kAssetsRevision[] = "${report.sourceRevisions?.assets ?? ''}";\ninline constexpr char kLocalesRevision[] = "${report.sourceRevisions?.locales ?? ''}";\nstruct Entity { const char* id; const char* name; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct Form { const char* id; const char* speciesId; const char* formKey; const char* name; uint8_t hp; uint8_t atk; uint8_t def; uint8_t spatk; uint8_t spdef; uint8_t speed; uint16_t ability1; uint16_t ability2; uint16_t abilityHidden; const char* type1; const char* type2; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct Species { uint16_t dex; uint16_t malePercentTenths; uint8_t generation; int8_t starterCost; bool starterEligible; uint16_t baseTotal; uint8_t hp; uint8_t atk; uint8_t def; uint8_t spatk; uint8_t spdef; uint8_t speed; uint16_t ability1; uint16_t ability2; uint16_t abilityHidden; uint16_t abilityPassive; uint32_t learnsetOffset; uint16_t learnsetCount; int8_t legendary; int8_t subLegendary; int8_t mythical; const char* growthRate; const char* id; const char* name; const char* type1; const char* type2; const char* firstFormId; const char* assetSourcePath; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nenum MoveCategory : uint8_t { MovePhysical = 0, MoveSpecial = 1, MoveStatus = 2 };\nstruct Move { uint16_t id; uint8_t category; int16_t power; int16_t accuracy; int16_t pp; int8_t priority; int16_t upstreamChance; uint8_t generation; const char* key; const char* name; const char* type; const char* target; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct SpeciesLevelMove { uint16_t speciesDex; uint8_t level; uint16_t moveId; };\nstruct SpeciesEvolution { const char* sourceSpeciesId; const char* targetSpeciesId; uint16_t level; int16_t strongThreshold; int16_t normalThreshold; int16_t wildThreshold; uint16_t sourceOrder; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct BiomeEncounterPoolEntry { const char* biomeId; const char* tier; const char* timeOfDay; uint16_t memberIndex; const char* speciesId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct BiomeTrainerPoolEntry { const char* biomeId; const char* tier; const char* trainerId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr Species kSpecies[] = {\n${speciesRows}\n};\ninline constexpr SpeciesEvolution kSpeciesEvolutions[] = {\n${evolutionRows}\n};\ninline constexpr Form kForms[] = {\n${forms}\n};\ninline constexpr Move kMoves[] = {\n${moves}\n};\ninline constexpr SpeciesLevelMove kSpeciesLevelMoves[] = {\n${levelMoves.map(move => `    {${move.dex}, ${move.level}, ${move.moveId}}`).join(',\n')}\n};\ninline constexpr Entity kAbilities[] = {\n${abilities}\n};\ninline constexpr Entity kItems[] = {\n${items}\n};\ninline constexpr Entity kModes[] = {\n${modes}\n};\ninline constexpr Entity kBiomes[] = {\n${biomes}\n};\ninline constexpr BiomeEncounterPoolEntry kBiomeEncounterPools[] = {\n${encounterPoolRows}\n};\ninline constexpr BiomeTrainerPoolEntry kBiomeTrainerPools[] = {\n${trainerPoolRows}\n};\ninline constexpr Entity kRoutes[] = {\n${routes}\n};\ninline constexpr Entity kLocales[] = {\n${locales}\n};\ninline constexpr std::size_t kSpeciesCount = sizeof(kSpecies) / sizeof(kSpecies[0]);\ninline constexpr std::size_t kSpeciesEvolutionCount = sizeof(kSpeciesEvolutions) / sizeof(kSpeciesEvolutions[0]);\ninline constexpr std::size_t kFormCount = sizeof(kForms) / sizeof(kForms[0]);\ninline constexpr std::size_t kMoveCount = sizeof(kMoves) / sizeof(kMoves[0]);\ninline constexpr std::size_t kSpeciesLevelMoveCount = sizeof(kSpeciesLevelMoves) / sizeof(kSpeciesLevelMoves[0]);\ninline constexpr std::size_t kAbilityCount = sizeof(kAbilities) / sizeof(kAbilities[0]);\ninline constexpr std::size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);\ninline constexpr std::size_t kModeCount = sizeof(kModes) / sizeof(kModes[0]);\ninline constexpr std::size_t kBiomeCount = sizeof(kBiomes) / sizeof(kBiomes[0]);\ninline constexpr std::size_t kBiomeEncounterPoolCount = sizeof(kBiomeEncounterPools) / sizeof(kBiomeEncounterPools[0]);\ninline constexpr std::size_t kBiomeTrainerPoolCount = sizeof(kBiomeTrainerPools) / sizeof(kBiomeTrainerPools[0]);\ninline constexpr std::size_t kRouteCount = sizeof(kRoutes) / sizeof(kRoutes[0]);\ninline constexpr std::size_t kLocaleCount = sizeof(kLocales) / sizeof(kLocales[0]);\ninline constexpr const Species* findSpeciesByDex(uint16_t dex) { for (const auto& species : kSpecies) if (species.dex == dex) return &species; return nullptr; }\ninline constexpr const Form* findFormById(const char* id) { if (!id) return nullptr; for (const auto& form : kForms) { const char* a = form.id; const char* b = id; while (*a && *b && *a == *b) { ++a; ++b; } if (*a == *b) return &form; } return nullptr; }\ninline constexpr const Move* findMoveById(uint16_t id) { for (const auto& move : kMoves) if (move.id == id) return &move; return nullptr; }\ninline constexpr const SpeciesLevelMove* levelMovesFor(const Species& species) { return species.learnsetCount ? &kSpeciesLevelMoves[species.learnsetOffset] : nullptr; }\nstatic_assert(findSpeciesByDex(6)->malePercentTenths == 875, "Charizard canonical gender ratio mismatch");\nstatic_assert(findSpeciesByDex(81)->malePercentTenths == 65534, "Magnemite genderless canonical data mismatch");\nstatic_assert(findSpeciesByDex(1)->ability2 == 65, "Bulbasaur normalized secondary ability mismatch");\nstatic_assert(findSpeciesByDex(16)->ability2 != findSpeciesByDex(16)->ability1, "Pidgey dual ability data mismatch");\nstatic_assert(findSpeciesByDex(1)->hp == 45 && findSpeciesByDex(1)->atk == 49 && findSpeciesByDex(1)->ability1 == 65, "Bulbasaur canonical battle data mismatch");\nstatic_assert(findMoveById(33)->power == 40 && findMoveById(33)->priority == 0, "Tackle canonical move data mismatch");\nstatic_assert(levelMovesFor(*findSpeciesByDex(1))[0].moveId == 33, "Bulbasaur canonical level-up moves mismatch");\ninline constexpr char kStartingBiomeId[] = "${field(content.extensions?.upstreamStartingBiome?.id ?? 'plains')}";\n}\n`;
const runtimeHeader = header
  .replace(
    'enum MoveCategory : uint8_t { MovePhysical = 0, MoveSpecial = 1, MoveStatus = 2 };',
    'enum MoveCategory : uint8_t { MovePhysical = 0, MoveSpecial = 1, MoveStatus = 2 }; enum MoveUpstreamFlags : uint8_t { MoveIsUnimplemented = 1, MoveHasSacrificialAttrOnHit = 2 };'
  )
  .replace(
    'struct SpeciesLevelMove { uint16_t speciesDex; uint8_t level; uint16_t moveId; };',
    'struct SpeciesLevelMove { uint16_t speciesDex; int8_t level; uint16_t moveId; };'
  )
  .replace(
    'struct Move { uint16_t id; uint8_t category; int16_t power; int16_t accuracy; int16_t pp; int8_t priority; int16_t upstreamChance; uint8_t generation; const char* key;',
    'struct Move { uint16_t id; uint8_t category; int16_t power; int16_t accuracy; int16_t pp; int8_t priority; int16_t upstreamChance; uint8_t generation; uint8_t upstreamFlags; const char* key;'
  );
await fs.mkdir(path.dirname(outputPath), { recursive: true });
await fs.writeFile(outputPath, runtimeHeader, 'utf8');
console.log(JSON.stringify({ output: path.relative(root, outputPath), bytes: Buffer.byteLength(runtimeHeader), hash: report.contentHash }));
