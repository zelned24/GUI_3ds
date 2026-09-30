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
const experienceRates = content.extensions?.pokemonExperience?.growthRates;
const growthRateNames = ['ERRATIC', 'FAST', 'MEDIUM_FAST', 'MEDIUM_SLOW', 'SLOW', 'FLUCTUATING'];
if (!experienceRates || growthRateNames.some(rate => !Array.isArray(experienceRates[rate]) || experienceRates[rate].length !== 100 || experienceRates[rate].some(value => !Number.isInteger(value) || value < 0 || value > 0xFFFFFFFF))) {
  throw new Error('Pinned Pokémon experience growth tables are missing or invalid');
}
const experienceRows = growthRateNames.map(rate => `    {${experienceRates[rate].join(', ')}}`).join(',\n');
const fixedBossContract = content.extensions?.classicFixedBossWaves;
const fixedBossEntries = fixedBossContract?.entries;
const classicFinalWave = fixedBossContract?.finalWave;
if (!Array.isArray(fixedBossEntries) || !fixedBossEntries.length ||
    fixedBossContract.provenance?.sourcePath !== 'src/enums/fixed-boss-waves.ts' ||
    !fixedBossContract.provenance?.sourceHash ||
    !Number.isInteger(classicFinalWave?.wave) || classicFinalWave.wave < 1 || classicFinalWave.wave > 0xFFFF ||
    classicFinalWave.provenance?.sourcePath !== 'src/game-mode.ts' || !classicFinalWave.provenance?.sourceHash) {
  throw new Error('Pinned Classic fixed-boss/final-wave data or provenance is missing');
}
const fixedBossWaves = [...fixedBossEntries].sort((left, right) => left.wave - right.wave);
const fixedBossSeen = new Set();
for (const entry of fixedBossWaves) {
  if (!Number.isInteger(entry.wave) || entry.wave < 1 || entry.wave > classicFinalWave.wave ||
      typeof entry.symbol !== 'string' || !/^[A-Z][A-Z0-9_]*$/.test(entry.symbol) ||
      fixedBossSeen.has(entry.wave)) {
    throw new Error(`Invalid or duplicate pinned Classic fixed-boss wave: ${JSON.stringify(entry)}`);
  }
  fixedBossSeen.add(entry.wave);
}
const fixedBossWaveRows = fixedBossWaves.map(entry =>
  `    {${entry.wave}, "${field(entry.symbol)}", "${field(fixedBossContract.provenance.sourcePath)}", "${field(fixedBossContract.provenance.sourceSymbol)}", "${field(fixedBossContract.provenance.sourceHash)}"}`
).join(',\n');
const fixedBattleContract = content.extensions?.classicFixedBattleWaves;
if (!Array.isArray(fixedBattleContract?.entries) || !fixedBattleContract.entries.length ||
    fixedBattleContract.provenance?.sourcePath !== 'src/data/trainers/fixed-battle-configs.ts' ||
    fixedBattleContract.provenance?.sourceSymbol !== 'classicFixedBattles' || !fixedBattleContract.provenance?.sourceHash) {
  throw new Error('Pinned Classic fixed-battle configuration or provenance is missing');
}
const fixedBattleEntries = [...fixedBattleContract.entries].sort((left, right) => left.wave - right.wave);
const fixedBattleSeen = new Set();
for (const entry of fixedBattleEntries) {
  if (!Number.isInteger(entry.wave) || entry.wave < 1 || entry.wave > classicFinalWave.wave ||
      typeof entry.symbol !== 'string' || !/^[A-Z][A-Z0-9_]*$/.test(entry.symbol) ||
      typeof entry.raw !== 'string' || !entry.raw.trim() ||
      (entry.trainerTypeId !== null && (!Number.isInteger(entry.trainerTypeId) || entry.trainerTypeId < 0 || entry.trainerTypeId > 65535)) ||
      (entry.trainerTypeId === null ? entry.trainerTypeSymbol !== null : typeof entry.trainerTypeSymbol !== 'string') ||
      (entry.trainerTypeId !== null && !content.collections.trainers.some(trainer => trainer.trainerTypeId === entry.trainerTypeId && trainer.id === entry.trainerTypeSymbol.toLowerCase())) ||
      typeof entry.seededBinaryGenderVariant !== 'boolean' ||
      (entry.seededBinaryGenderVariant && entry.trainerTypeId === null) ||
      !fixedBossWaves.some(boss => boss.wave === entry.wave && boss.symbol === entry.symbol) ||
      fixedBattleSeen.has(entry.wave)) {
    throw new Error(`Invalid or unresolved pinned Classic fixed-battle wave: ${JSON.stringify(entry)}`);
  }
  fixedBattleSeen.add(entry.wave);
}
const fixedBattleWaveRows = fixedBattleEntries.map(entry =>
  `    {${entry.wave}, ${entry.trainerTypeId ?? 0}, ${entry.trainerTypeId !== null}, ${entry.seededBinaryGenderVariant}, "${field(entry.symbol)}", "${field(fixedBattleContract.provenance.sourcePath)}", "${field(fixedBattleContract.provenance.sourceSymbol)}[ClassicFixedBossWaves.${field(entry.symbol)}]", "${field(fixedBattleContract.provenance.sourceHash)}"}`
).join(',\n');
const classicFinalWaveProvenance = classicFinalWave.provenance;
const defaultStarterIds = content.extensions?.freshProfile?.defaultStarterSpecies;
if (!Array.isArray(defaultStarterIds) || !defaultStarterIds.length || !content.extensions?.freshProfile?.provenance?.sourceHash) {
  throw new Error('Pinned fresh-profile starter metadata is missing');
}
const defaultStarterSet = new Set(defaultStarterIds);
if (defaultStarterSet.size !== defaultStarterIds.length || defaultStarterIds.some(id => !collections.species.some(species => species.id === id))) {
  throw new Error('Pinned fresh-profile starter metadata has duplicate or unresolved species IDs');
}
const defaultStarterOrder = new Map(defaultStarterIds.map((id, index) => [id, index]));
const speciesIds = new Set(collections.species.map(item => item.id));
const speciesById = new Map(collections.species.map(item => [item.id, item]));
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
const formLearnsetRanges = new Map();
for (const form of collections.forms) {
  const species = speciesById.get(form.speciesId);
  if (!species) throw new Error(`Canonical form references missing species: ${form.id} -> ${form.speciesId}`);
  const offset = levelMoves.length;
  for (const learned of form.levelMoves ?? []) {
    const move = movesById.get(learned.move ?? learned.id);
    if (!move || !Number.isInteger(move.moveId) || move.moveId < 1) {
      throw new Error(`Invalid canonical form learnset reference: ${form.id} -> ${learned.move ?? learned.id}`);
    }
    if (!Number.isInteger(learned.level) || learned.level < -1 || learned.level > 127) {
      throw new Error(`Invalid canonical form learnset level: ${form.id} -> ${move.id}`);
    }
    levelMoves.push({ dex: species.nationalDexId, level: learned.level, moveId: move.moveId });
  }
  formLearnsetRanges.set(form.id, { offset, count: levelMoves.length - offset });
  if (levelMoves.length - offset > 0xFFFF) throw new Error(`Canonical form learnset for ${form.id} exceeds 16-bit count range`);
}
if (levelMoves.length > 0xFFFFFFFF) throw new Error('Canonical learnset table exceeds 32-bit offset range');
const eggMoves = [];
const eggMoveRanges = new Map();
for (const species of collections.species) {
  const offset = eggMoves.length;
  for (const eggMove of species.eggMoves ?? []) {
    const move = movesById.get(typeof eggMove === 'string' ? eggMove : eggMove.id ?? eggMove.move);
    if (!move || !Number.isInteger(move.moveId) || move.moveId < 1) {
      throw new Error(`Invalid canonical egg move reference: ${species.id} -> ${eggMove}`);
    }
    eggMoves.push({ dex: species.nationalDexId, moveId: move.moveId });
  }
  eggMoveRanges.set(species.id, { offset, count: eggMoves.length - offset });
  if (eggMoves.length - offset > 0xFFFF) throw new Error(`Canonical egg-move count exceeds 16-bit range: ${species.id}`);
}
if (eggMoves.length > 0xFFFFFFFF) throw new Error('Canonical egg-move table exceeds 32-bit offset range');
const evolutionEdges = collections.species.flatMap(species => (species.evolutions ?? []).map((edge, order) => {
  if (!edge.targetSpeciesId || !speciesIds.has(edge.targetSpeciesId)) {
    throw new Error(`Invalid canonical evolution link: ${species.id} -> ${edge.targetSpeciesId ?? '(missing)'}`);
  }
  if (!Number.isFinite(edge.level)) throw new Error(`Invalid canonical evolution level: ${species.id} -> ${edge.targetSpeciesId}`);
  return { species, edge, order };
}));
const evolutionRows = evolutionEdges.map(({ species, edge, order }) => `    {"${field(species.id)}", "${field(edge.targetSpeciesId)}", ${edge.level}, ${edge.evoLevelThreshold?.strong ?? -1}, ${edge.evoLevelThreshold?.normal ?? -1}, ${edge.evoLevelThreshold?.wild ?? -1}, ${order}, "${field(edge.source?.sourcePath ?? species.source?.sourcePath ?? '')}", "${field(edge.source?.sourceSymbol ?? '')}", "${field(edge.source?.sourceHash ?? species.source?.sourceHash ?? '')}", "${field(edge.preFormKey)}", "${field(edge.evoFormKey)}"}`).join(',\n');
const spriteAtlasFrames = [];
const spriteAtlases = [];
for (const asset of [...collections.assetReferences].sort((a, b) => a.speciesId - b.speciesId)) {
  if (!asset.manifestVerified) continue;
  const validSize = value => Number.isInteger(value) && value > 0 && value <= 0xFFFF;
  if (!validSize(asset.atlasWidth) || !validSize(asset.atlasHeight)
      || !Array.isArray(asset.frames) || !asset.frames.length || asset.frames.length > 0xFFFF
      || !asset.sourcePath || !asset.sourceHash || !asset.imagePath
      || !collections.species.some(species => species.nationalDexId === asset.speciesId))
    throw new Error(`Invalid canonical sprite atlas: ${asset.speciesId}`);
  const offset = spriteAtlasFrames.length;
  for (const frame of asset.frames) {
    const rect = frame.frame, source = frame.sourceSize, trim = frame.spriteSourceSize;
    if (!frame.filename || ![rect?.x, rect?.y, rect?.w, rect?.h, source?.w, source?.h,
      trim?.x, trim?.y].every(value => Number.isInteger(value) && value >= 0 && value <= 0xFFFF)
        || rect.w < 1 || rect.h < 1 || source.w < 1 || source.h < 1
        || rect.x + rect.w > asset.atlasWidth || rect.y + rect.h > asset.atlasHeight
        || trim.x + rect.w > source.w || trim.y + rect.h > source.h)
      throw new Error(`Invalid canonical sprite frame: ${asset.speciesId}.${frame.filename}`);
    spriteAtlasFrames.push({ ...frame, rect, source, trim });
  }
  spriteAtlases.push({ ...asset, offset, count: asset.frames.length });
}
const spriteAtlasRows = spriteAtlases.map(atlas => `    {${atlas.speciesId}, ${atlas.atlasWidth}, ${atlas.atlasHeight}, ${atlas.offset}, ${atlas.count}, "${field(atlas.sourcePath)}", "${field(atlas.imagePath)}", "${field(atlas.sourceHash)}"}`).join(',\n');
const spriteFrameRows = spriteAtlasFrames.map(({ filename, rect, source, trim }) => `    {"${field(filename)}", ${rect.x}, ${rect.y}, ${rect.w}, ${rect.h}, ${source.w}, ${source.h}, ${trim.x}, ${trim.y}}`).join(',\n');
const speciesRows = collections.species.map(item => {
  const asset = collections.assetReferences.find(entry => entry.speciesId === item.nationalDexId) ?? item.extensions?.assetReference;
  const form = collections.forms.find(entry => entry.speciesId === item.id);
  const stats = item.baseStats;
  if (!Number.isInteger(item.baseExp) || item.baseExp < 0 || item.baseExp > 0xFFFF) throw new Error(`Invalid or missing upstream baseExp for ${item.id}`);
  for (const key of ['hp', 'atk', 'def', 'spatk', 'spdef', 'spd']) {
    if (!Number.isInteger(stats?.[key]) || stats[key] < 0 || stats[key] > 255) throw new Error(`Invalid canonical base stat ${item.id}.${key}`);
  }
  const learnset = learnsetRanges.get(item.id);
  const eggMoveRange = eggMoveRanges.get(item.id);
  const abilityData = item.abilities ?? {};
  const primaryAbilityId = abilityIdFor(abilityData.primary ?? abilityData.ability1);
  const secondaryAbilityId = abilityIdFor(abilityData.secondary ?? abilityData.ability2) || primaryAbilityId;
  const flag = value => value === true ? 1 : value === false ? 0 : -1;
  const malePercentTenths = item.malePercent === null ? 0xFFFE
    : Number.isFinite(item.malePercent) ? Math.round(item.malePercent * 10) : 0xFFFF;
  if (malePercentTenths !== 0xFFFE && malePercentTenths !== 0xFFFF && (malePercentTenths < 0 || malePercentTenths > 1000)) {
    throw new Error(`Canonical malePercent exceeds compact runtime range: ${item.id}`);
  }
  const prevolution = item.prevolutionSpeciesId == null ? null : speciesById.get(item.prevolutionSpeciesId);
  if (item.extensions?.prevolution?.status !== 'RESOLVED' || (item.prevolutionSpeciesId != null && !prevolution)) {
    throw new Error(`Unresolved canonical prevolution for ${item.id}`);
  }
  const freshStarterOrdinal = defaultStarterOrder.get(item.id) ?? 255;
  return `    {${item.nationalDexId}, ${prevolution?.nationalDexId ?? 0}, ${item.baseExp}, ${malePercentTenths}, ${item.generation ?? 0}, ${item.starterCost ?? -1}, ${item.starterEligible === true ? 'true' : 'false'}, ${defaultStarterSet.has(item.id) ? 'true' : 'false'}, ${freshStarterOrdinal}, ${item.baseTotal ?? 0}, ${stats.hp}, ${stats.atk}, ${stats.def}, ${stats.spatk}, ${stats.spdef}, ${stats.spd}, ${primaryAbilityId}, ${secondaryAbilityId}, ${abilityIdFor(abilityData.hidden ?? abilityData.abilityHidden)}, ${abilityIdFor(abilityData.passive)}, ${learnset.offset}, ${learnset.count}, ${eggMoveRange.offset}, ${eggMoveRange.count}, ${flag(item.rarity?.legendary)}, ${flag(item.rarity?.subLegendary)}, ${flag(item.rarity?.mythical)}, "${field(item.growthRate ?? '')}", "${field(item.id)}", "${field(item.name)}", "${field(item.type1)}", "${field(item.type2)}", "${field(form?.id ?? '')}", "${field(asset?.sourcePath ?? '')}", "${field(item.source?.sourcePath ?? '')}", "${field(item.source?.sourceSymbol ?? '')}", "${field(item.source?.sourceHash ?? '')}"}`;
}).join(',\n');
const localeRows = collections.locales
  .filter(item => ['pokemon', 'move', 'ability', 'item', 'modifier-type', 'gameMode', 'biomes', 'trainer-classes', 'trainer-names'].includes(item.namespace))
  .map(item => ({ id: `${item.locale}:${item.namespace}:${item.canonicalId}`, name: item.value?.name ?? (typeof item.value === 'string' ? item.value : ''), provenance: item.source }));
const locales = entityRows(localeRows);
const modes = entityRows(collections.gameModes);
const biomes = entityRows(collections.biomes.map(item => ({
  ...item,
  name: collections.locales.find(entry => entry.namespace === 'biomes' && entry.canonicalId === item.id && entry.locale === 'en')?.value?.name ?? item.id,
})));
const biomeEncounterPools = collections.biomes.flatMap(biome => Object.entries(biome.encounterPools ?? {}).flatMap(([tier, times]) => Object.entries(times).flatMap(([time, speciesIds]) => speciesIds.map((speciesId, memberIndex) => ({ biome, tier, time, speciesId, memberIndex }))))).sort((a, b) => a.biome.id.localeCompare(b.biome.id) || a.tier.localeCompare(b.tier) || a.time.localeCompare(b.time));
const encounterPoolRows = biomeEncounterPools.map(({ biome, tier, time, speciesId, memberIndex }) => `    {"${field(biome.id)}", "${field(tier)}", "${field(time)}", ${memberIndex}, "${field(speciesId)}", "${field(biome.provenance.sourcePath)}", "${field(biome.provenance.sourceSymbol)}.pokemonPool.${field(tier)}.${field(time)}", "${field(biome.provenance.sourceHash)}"}`).join(',\n');
const biomeTrainerPools = collections.biomes.flatMap(biome => Object.entries(biome.trainerPools ?? {}).flatMap(([tier, trainerIds]) => trainerIds.map((trainerId, memberIndex) => ({ biome, tier, trainerId, memberIndex })))).sort((a, b) => a.biome.id.localeCompare(b.biome.id) || a.tier.localeCompare(b.tier) || a.memberIndex - b.memberIndex);
const trainerPoolRows = biomeTrainerPools.map(({ biome, tier, trainerId, memberIndex }) => `    {"${field(biome.id)}", "${field(tier)}", ${memberIndex}, "${field(trainerId)}", "${field(biome.provenance.sourcePath)}", "${field(biome.provenance.sourceSymbol)}.trainerPool.${field(tier)}", "${field(biome.provenance.sourceHash)}"}`).join(',\n');
if (!Array.isArray(collections.trainers) || !collections.trainers.length) throw new Error('Pinned trainer type catalog is missing from canonical content');
const trainerTypeIds = new Set(collections.trainers.map(trainer => trainer.id));
const trainerTypesSorted = [...collections.trainers].sort((a, b) => a.trainerTypeId - b.trainerTypeId);
for (const trainer of trainerTypesSorted) {
  const source = trainer.source ?? trainer.metadata ?? {};
  if (!Number.isInteger(trainer.trainerTypeId) || trainer.trainerTypeId < 0 || trainer.trainerTypeId > 0xFFFF ||
      typeof trainer.id !== 'string' || !trainer.id || typeof trainer.extensions?.configStatus !== 'string' ||
      !source.sourcePath || !source.sourceSymbol || !source.sourceHash) {
    throw new Error(`Invalid pinned trainer type definition: ${trainer.id}`);
  }
}
for (const { biome, tier, trainerId } of biomeTrainerPools) if (!trainerTypeIds.has(trainerId)) throw new Error(`Biome trainer pool references missing TrainerType: ${biome.id}.${tier} -> ${trainerId}`);
if (!Array.isArray(collections.trainerPartyTemplates) || !collections.trainerPartyTemplates.length) throw new Error('Pinned trainer party template catalog is missing from canonical content');
const trainerPartySegments = [];
const trainerPartyTemplateRows = [...collections.trainerPartyTemplates].sort((a, b) => a.templateKey.localeCompare(b.templateKey)).map(template => {
  const source = template.source ?? template.metadata ?? {};
  if (!template.templateKey || !Number.isInteger(template.totalSize) || template.totalSize < 1 || template.totalSize > 6 || typeof template.isCompound !== 'boolean' ||
      !Array.isArray(template.segments) || !template.segments.length || !source.sourcePath || !source.sourceSymbol || !source.sourceHash ||
      typeof template.extensions?.upstreamRawRecord?.value !== 'string') {
    throw new Error(`Invalid or unprovenanced pinned trainer party template: ${template.id}`);
  }
  const offset = trainerPartySegments.length;
  let totalSize = 0;
  for (const segment of template.segments) {
    if (!Number.isInteger(segment.size) || segment.size < 1 || segment.size > 6 ||
        !Number.isInteger(segment.strengthId) || segment.strengthId < 0 || segment.strengthId > 0xFF ||
        typeof segment.sameSpecies !== 'boolean' || typeof segment.balanced !== 'boolean' ||
        !Number.isInteger(segment.evolutionThresholdKindId) || segment.evolutionThresholdKindId < 0 || segment.evolutionThresholdKindId > 0xFF ||
        !['NORMAL', 'STRONG'].includes(segment.evolutionThresholdKind)) {
      throw new Error(`Invalid normalized trainer party segment: ${template.templateKey}`);
    }
    totalSize += segment.size;
    trainerPartySegments.push(segment);
  }
  if (totalSize !== template.totalSize || totalSize > 6) throw new Error(`Trainer party template slot count mismatch: ${template.templateKey}`);
  if (trainerPartySegments.length - offset > 0xFFFF) throw new Error(`Trainer party template segment range overflow: ${template.templateKey}`);
  return { template, source, offset, count: trainerPartySegments.length - offset };
});
if (trainerPartySegments.length > 0xFFFFFFFF) throw new Error('Trainer party segment table exceeds 32-bit offset range');
const trainerPartySegmentRows = trainerPartySegments.map(segment =>
  `    {${segment.size}, ${segment.strengthId}, ${segment.sameSpecies ? 'true' : 'false'}, ${segment.balanced ? 'true' : 'false'}, ${segment.evolutionThresholdKindId}, "${field(segment.strength)}", "${field(segment.evolutionThresholdKind)}"}`
).join(',\n');
const trainerPartyTableRows = trainerPartyTemplateRows.map(({ template, source, offset, count }) =>
  `    {"${field(template.templateKey)}", ${template.totalSize}, ${template.isCompound ? 'true' : 'false'}, ${offset}, ${count}, "${field(source.sourcePath)}", "${field(source.sourceSymbol)}", "${field(source.sourceHash)}"}`
).join(',\n');
const trainerPartyTemplateKeys = new Set(trainerPartyTemplateRows.map(row => row.template.templateKey));
const trainerPartyTemplateRefs = [];
const trainerPoolRowsNative = [];
const trainerPoolChoices = [];
const trainerPoolSpecies = [];
const trainerSignatureChoices = [];
const trainerSignatureSpecies = [];
const trainerTypeRows = trainerTypesSorted.map(trainer => {
  const source = trainer.source ?? trainer.metadata ?? {};
  const configSource = trainer.extensions?.upstreamConfig ?? {};
  const missingConfig = trainer.extensions?.configStatus === 'MISSING_IN_UPSTREAM';
  if ((!missingConfig && (!Number.isFinite(trainer.moneyMultiplier) || trainer.moneyMultiplier < 0 || trainer.moneyMultiplier > 0xFFFFFFFF / 1000)) ||
      (missingConfig && trainer.moneyMultiplier !== null) ||
      !Array.isArray(trainer.partyTemplateKeys) || !Array.isArray(trainer.speciesPools)) {
    throw new Error(`Invalid normalized trainer config for ${trainer.id}`);
  }
  const derivedType = trainer.trainerRules?.derivedTrainerTypeId;
  if (!Number.isInteger(derivedType) || !trainerTypesSorted.some(entry => entry.trainerTypeId === derivedType)) throw new Error(`Invalid derived trainer type: ${trainer.id}`);
  const partyOffset = trainerPartyTemplateRefs.length;
  for (const key of trainer.partyTemplateKeys) {
    if (!trainerPartyTemplateKeys.has(key)) throw new Error(`Trainer ${trainer.id} references missing party template ${key}`);
    trainerPartyTemplateRefs.push({ trainerId: trainer.trainerTypeId, key, maxWave: 0 });
  }
  if (trainerPartyTemplateRefs.length - partyOffset > 0xFFFF) throw new Error(`Trainer party-template reference range overflow: ${trainer.id}`);
  const callbackOffset = trainerPartyTemplateRefs.length;
  const callbackKeys = trainer.trainerRules?.callbackTemplateKeys ?? [];
  if (!Array.isArray(callbackKeys) || callbackKeys.length > 0xFFFF) throw new Error(`Invalid trainer callback templates: ${trainer.id}`);
  const wavePolicy = trainer.trainerRules?.callbackWavePolicy;
  if (wavePolicy && (wavePolicy.modeId !== 'classic' || !Array.isArray(wavePolicy.ranges) || wavePolicy.ranges.length !== callbackKeys.length)) throw new Error(`Invalid callback wave policy: ${trainer.id}`);
  let previousMaxWave = 0;
  for (const [index, key] of callbackKeys.entries()) {
    if (!trainerPartyTemplateKeys.has(key)) throw new Error(`Trainer ${trainer.id} references missing callback template ${key}`);
    const range = wavePolicy?.ranges[index];
    const maxWave = range?.maxWave ?? 0;
    if (wavePolicy && (range.templateKey !== key || (index === callbackKeys.length - 1 ? range.maxWave !== null : !Number.isInteger(maxWave) || maxWave <= previousMaxWave || maxWave > 65535))) throw new Error(`Invalid callback wave range: ${trainer.id}`);
    previousMaxWave = maxWave;
    trainerPartyTemplateRefs.push({ trainerId: trainer.trainerTypeId, key, maxWave });
  }
  const poolOffset = trainerPoolRowsNative.length;
  for (const pool of [...trainer.speciesPools].sort((a, b) => a.tierId - b.tierId)) {
    if (pool.candidates?.length > 0xFFFF) throw new Error(`Trainer species-pool candidate range overflow: ${trainer.id}`);
    if (!Number.isInteger(pool.tierId) || pool.tierId < 0 || pool.tierId > 0xFF || !Array.isArray(pool.candidates)) throw new Error(`Invalid normalized trainer species pool: ${trainer.id}`);
    const candidateOffset = trainerPoolChoices.length;
    for (const candidate of pool.candidates) {
      if (typeof candidate.isGroup !== 'boolean' || !Array.isArray(candidate.speciesIds) || !candidate.speciesIds.length || candidate.speciesIds.length > 0xFF || (!candidate.isGroup && candidate.speciesIds.length !== 1)) throw new Error(`Invalid trainer species choice group: ${trainer.id}.${pool.tier}`);
      const speciesOffset = trainerPoolSpecies.length;
      for (const speciesId of candidate.speciesIds) {
        if (!speciesById.has(speciesId)) throw new Error(`Trainer species pool references missing species: ${trainer.id}.${pool.tier} -> ${speciesId}`);
        trainerPoolSpecies.push(speciesId);
      }
      trainerPoolChoices.push({ memberIndex: pool.candidates.indexOf(candidate), speciesOffset, speciesCount: candidate.speciesIds.length, isGroup: candidate.isGroup });
    }
    trainerPoolRowsNative.push({ trainerId: trainer.trainerTypeId, tierId: pool.tierId, tier: pool.tier, candidateOffset, candidateCount: pool.candidates.length, sourcePath: configSource.sourcePath ?? '', sourceSymbol: configSource.sourceSymbol ?? '', sourceHash: configSource.sourceHash ?? '' });
  }
  if (trainerPoolRowsNative.length - poolOffset > 0xFFFF) throw new Error(`Trainer species-pool range overflow: ${trainer.id}`);
  const signatureOffset = trainerSignatureChoices.length;
  const signature = trainer.trainerRules?.signatureSpecies ?? [];
  const signatureSource = trainer.trainerRules?.signatureSpeciesProvenance;
  if (!Array.isArray(signature) || signature.length > 6 || (signature.length && (!signatureSource?.sourcePath || !signatureSource?.sourceHash || !signatureSource?.sourceSymbol))) throw new Error(`Invalid trainer signature provenance: ${trainer.id}`);
  for (const [slot, choice] of signature.entries()) {
    if (choice.slot !== slot || typeof choice.isGroup !== 'boolean' || !Array.isArray(choice.speciesIds)
        || !choice.speciesIds.length || choice.speciesIds.length > 255 || (!choice.isGroup && choice.speciesIds.length !== 1)) throw new Error(`Invalid signature choice: ${trainer.id}[${slot}]`);
    const speciesOffset = trainerSignatureSpecies.length;
    for (const speciesId of choice.speciesIds) {
      if (!speciesById.has(speciesId)) throw new Error(`Missing trainer signature species: ${trainer.id} -> ${speciesId}`);
      trainerSignatureSpecies.push(speciesId);
    }
    trainerSignatureChoices.push({ trainerId: trainer.trainerTypeId, slot, isGroup: choice.isGroup, speciesOffset, speciesCount: choice.speciesIds.length,
      sourcePath: signatureSource.sourcePath, sourceSymbol: signatureSource.sourceSymbol, sourceHash: signatureSource.sourceHash });
  }
  const flags = (trainer.trainerRules?.isBoss === true ? 1 : 0) |
    (trainer.trainerRules?.hasDouble === true ? 2 : 0) |
    (trainer.trainerRules?.doubleOnly === true ? 4 : 0) |
    (trainer.trainerRules?.hasStaticParty === true ? 8 : 0) |
    (trainer.trainerRules?.useSameSeedForAllMembers === true ? 16 : 0) |
    (trainer.trainerRules?.signatureCallbackInstalled === true ? 32 : 0);
  const configStatus = trainer.trainerRules?.partyTemplateStatus ?? 'MISSING_IN_UPSTREAM';
  const templateStatus = trainer.trainerRules?.templateResolutionStatus ?? configStatus;
  return `    {${trainer.trainerTypeId}, ${derivedType}, ${missingConfig ? 0 : Math.round(trainer.moneyMultiplier * 1000)}, ${flags}, ${partyOffset}, ${trainer.partyTemplateKeys.length}, ${callbackOffset}, ${callbackKeys.length}, ${signatureOffset}, ${signature.length}, ${poolOffset}, ${trainerPoolRowsNative.length - poolOffset}, "${field(trainer.id)}", "${field(trainer.name)}", "${field(templateStatus)}", "${field(configStatus)}", "${field(source.sourcePath)}", "${field(source.sourceSymbol)}", "${field(source.sourceHash)}", "${field(configSource.sourcePath ?? '')}", "${field(configSource.sourceSymbol ?? '')}", "${field(configSource.sourceHash ?? '')}"}`;
}).join(',\n');
const trainerSignatureSpeciesRows = trainerSignatureSpecies.map(id => `    {"${field(id)}"}`).join(',\n');
const trainerSignatureChoiceRows = trainerSignatureChoices.map(choice => `    {${choice.trainerId}, ${choice.slot}, ${choice.isGroup ? 'true' : 'false'}, ${choice.speciesOffset}, ${choice.speciesCount}, "${field(choice.sourcePath)}", "${field(choice.sourceSymbol)}", "${field(choice.sourceHash)}"}`).join(',\n');
const trainerPartyTemplateRefRows = trainerPartyTemplateRefs.map(ref => `    {${ref.trainerId}, "${field(ref.key)}", ${ref.maxWave}}`).join(',\n');
const trainerPoolSpeciesRows = trainerPoolSpecies.map(id => `    {"${field(id)}"}`).join(',\n');
const trainerPoolChoiceRows = trainerPoolChoices.map(choice => `    {${choice.memberIndex}, ${choice.speciesOffset}, ${choice.speciesCount}, ${choice.isGroup ? 'true' : 'false'}}`).join(',\n');
const trainerSpeciesPoolRows = trainerPoolRowsNative.map(pool => `    {${pool.trainerId}, ${pool.tierId}, ${pool.candidateOffset}, ${pool.candidateCount}, "${field(pool.tier)}", "${field(pool.sourcePath)}", "${field(pool.sourceSymbol)}", "${field(pool.sourceHash)}"}`).join(',\n');
const biomeTrainerChances = collections.biomes.map(biome => {
  const chance = biome.trainerChance;
  const source = biome.extensions?.upstreamTrainerChance;
  if (!Number.isSafeInteger(chance) || chance < 0 || chance > 0xFFFF || source?.value !== chance || source.sourcePath !== biome.provenance?.sourcePath || !source.sourceSymbol?.endsWith('.trainerChance') || source.sourceHash !== biome.provenance?.sourceHash) {
    throw new Error(`Invalid or unprovenanced upstream trainerChance for biome ${biome.id}`);
  }
  return { biome, chance, source };
}).sort((a, b) => a.biome.id.localeCompare(b.biome.id));
const biomeTrainerChanceRows = biomeTrainerChances.map(({ biome, chance, source }) => `    {"${field(biome.id)}", ${chance}, "${field(source.sourcePath)}", "${field(source.sourceSymbol)}", "${field(source.sourceHash)}"}`).join(',\n');
const routes = collections.routes.map(route => {
  if (!route.id || !route.from || !route.to) throw new Error(`Invalid canonical route: ${route.id}`);
  if (route.weight !== null && (!Number.isInteger(route.weight) || route.weight < 1 || route.weight > 0xFFFF)) {
    throw new Error(`Invalid canonical route weight: ${route.id}`);
  }
  if (route.conditions !== null) throw new Error(`Unsupported required route conditions: ${route.id}`);
  const source = route.provenance ?? {};
  return `    {"${field(route.id)}", "${field(route.from)}", "${field(route.to)}", ${route.weight ?? 0}, "${field(source.sourcePath ?? '')}", "${field(source.sourceSymbol ?? '')}", "${field(source.sourceHash ?? '')}"}`;
}).join(',\n');
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
  const learnset = formLearnsetRanges.get(form.id);
  return `    {"${field(form.id)}", "${field(form.speciesId)}", "${field(form.formKey)}", "${field(form.spriteAtlasKey ?? '')}", "${field(form.name)}", ${stats.hp}, ${stats.atk}, ${stats.def}, ${stats.spatk}, ${stats.spdef}, ${stats.spd}, ${abilities[0]}, ${abilities[1]}, ${abilities[2]}, ${learnset.offset}, ${learnset.count}, "${field(form.types[0])}", "${field(form.types[1] ?? '')}", "${field(form.provenance?.sourcePath ?? '')}", "${field(form.provenance?.sourceSymbol ?? '')}", "${field(form.provenance?.sourceHash ?? '')}"}`;
}).join(',\n');
const moveAttributes = [];
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
  const sourceAttributes = Array.isArray(move.upstreamAttributes)
    ? move.upstreamAttributes
    : [...upstreamRaw.matchAll(/\.attr\s*\(\s*(?:new\s+)?([A-Za-z_$][\w$]*)/g)].map(attribute => attribute[1]);
  if (sourceAttributes.some(attribute => typeof attribute !== 'string' || !/^[A-Za-z_$][\w$]*$/.test(attribute))) {
    throw new Error(`Invalid canonical move attribute list: ${move.id}`);
  }
  const attributeOffset = moveAttributes.length;
  if (sourceAttributes.length > 0xFFFF || attributeOffset > 0xFFFFFFFF - sourceAttributes.length) {
    throw new Error(`Move attribute range exceeds native representation: ${move.id}`);
  }
  moveAttributes.push(...sourceAttributes);
  const upstreamFlags = [];
  if (move.isUnimplemented || /\.unimplemented\s*\(/.test(upstreamRaw)) upstreamFlags.push('MoveIsUnimplemented');
  if (/SacrificialAttrOnHit/.test(upstreamRaw)) upstreamFlags.push('MoveHasSacrificialAttrOnHit');
  if (/SacrificialAttr/.test(upstreamRaw)) upstreamFlags.push('MoveHasSacrificialAttr');
  if (move.extensions?.upstreamMoveGeneration?.stabBlacklisted === true) upstreamFlags.push('MoveIsStabBlacklisted');
  if (/\.attr\s*\(\s*MultiHitAttr/.test(upstreamRaw)) upstreamFlags.push('MoveHasMultiHit');
  if (/MultiHitPowerIncrementAttr/.test(upstreamRaw)) upstreamFlags.push('MoveHasMultiHitPowerIncrement');
  if (/DelayedAttackAttr/.test(upstreamRaw)) upstreamFlags.push('MoveHasDelayedAttack');
  if (/RechargeAttr/.test(upstreamRaw)) upstreamFlags.push('MoveHasRecharge');
  if (/^\s*new\s+Charging\w*Move\s*\(/.test(upstreamRaw)) upstreamFlags.push('MoveIsCharging');
  if (/\.checkAllHits\s*\(/.test(upstreamRaw)) upstreamFlags.push('MoveChecksAccuracyPerHit');
  if (/DefAtkAttr/.test(upstreamRaw)) upstreamFlags.push('MoveUsesDefense');
  if (/PhotonGeyserCategoryAttr|ShellSideArmCategoryAttr/.test(upstreamRaw)) upstreamFlags.push('MoveSelectsOffensiveCategory');
  const multiHitDeclaration = upstreamRaw.match(/\.attr\s*\(\s*MultiHitAttr\s*(?:,\s*([^,)]+))?\s*\)/);
  const multiHitExpression = multiHitDeclaration?.[1]?.trim();
  const multiHitType = multiHitExpression
    ? multiHitExpression.match(/^MultiHitType\.(TWO_TO_FIVE|TWO|THREE|TEN|BEAT_UP)$/)?.[1]
    : (multiHitDeclaration ? 'TWO_TO_FIVE' : 'None');
  if (upstreamFlags.includes('MoveHasMultiHit') && !multiHitType) throw new Error(`Unsupported MultiHitAttr type: ${move.id}`);
  const multiHitValue = { None: 0, TWO_TO_FIVE: 1, TWO: 2, THREE: 3, TEN: 4, BEAT_UP: 5 }[multiHitType ?? 'None'];
  return `    {${move.moveId}, ${category}, ${move.power}, ${move.accuracy}, ${move.pp}, ${move.priority}, ${move.extensions?.upstreamChance ?? 0}, ${move.extensions?.upstreamGeneration ?? 0}, ${upstreamFlags.join(' | ') || 0}, ${multiHitValue}, ${attributeOffset}, ${sourceAttributes.length}, "${field(move.id)}", "${field(move.names?.en ?? move.name)}", "${field(move.type)}", "${field(move.target ?? '')}", "${field(move.source?.sourcePath ?? '')}", "${field(move.source?.sourceSymbol ?? '')}", "${field(move.source?.sourceHash ?? '')}"}`;
}).join(',\n');
const abilities = entityRows(collections.abilities);
const abilityMovegenProfiles = collections.abilities.map(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const aiOffsets = [...raw.matchAll(/AiMovegenMoveStatsAbAttr/g)].map(match => match.index);
  const aiOffset = aiOffsets[0] ?? -1;
  const callbackEnd = aiOffset < 0 ? -1 : raw.indexOf('.build()', aiOffset);
  const callback = aiOffset < 0 ? '' : raw.slice(aiOffset, callbackEnd < 0 ? raw.length : callbackEnd);
  const hasMaxMultiHit = /\.attr\(MaxMultiHitAbAttr\)/.test(raw) || /maxMultiHit\.value\s*=\s*true/.test(callback);
  const hasInstantCharge = /\.attr\(InstantChargeAbAttr\)/.test(raw);
  let flags = (hasMaxMultiHit ? 2 : 0) | (hasInstantCharge ? 4 : 0);
  let accuracyMultiplier = 1;
  if (aiOffsets.length > 1) flags |= 1;
  if (aiOffset >= 0 && aiOffsets.length === 1) {
    const bodyMatch = callback.match(/=>\s*\{([\s\S]*?)\}\s*\)\s*$/);
    const callbackBody = bodyMatch?.[1]?.trim() ?? '';
    const accuracy = callback.match(/accMult\.value\s*\*=\s*(\d+(?:\.\d+)?)/);
    const accuracyOnly = /^accMult\.value\s*\*=\s*\d+(?:\.\d+)?\s*;?\s*$/.test(callbackBody);
    const maxMultiHitOnly = /^maxMultiHit\.value\s*=\s*true\s*;?\s*$/.test(callbackBody);
    const recognized = (accuracyOnly && accuracy) || (maxMultiHitOnly && hasMaxMultiHit);
    if (recognized && accuracyOnly) accuracyMultiplier = Number(accuracy[1]);
    else if (!recognized) flags |= 1;
  }
  if (!Number.isInteger(ability.abilityId) || ability.abilityId < 0 || ability.abilityId > 0xFFFF) throw new Error(`Invalid canonical ability ID: ${ability.id}`);
  return { abilityId: ability.abilityId, flags, accuracyMultiplier, source: ability.source };
}).sort((left, right) => left.abilityId - right.abilityId);
const abilityMovegenProfileRows = abilityMovegenProfiles.map(profile =>
  `    {${profile.abilityId}, ${profile.flags}, ${profile.accuracyMultiplier}, "${field(profile.source?.sourcePath ?? '')}", "${field(profile.source?.sourceSymbol ?? '')}", "${field(profile.source?.sourceHash ?? '')}"}`
).join(',\n');
const items = entityRows(collections.items);
const modifierPoolEntries = content.extensions?.modifierPools?.entries ?? [];
const itemIds = new Set(collections.items.map(item => item.id));
for (const entry of modifierPoolEntries) {
  if (!itemIds.has(entry.itemId)) throw new Error(`Modifier pool references missing canonical item ${entry.itemId}`);
}
const modifierPoolRows = modifierPoolEntries.map(entry =>
  `    {"${field(entry.pool)}", "${field(entry.tier)}", ${entry.slot}, "${field(entry.itemId)}", ${entry.weight ?? -1}, ${entry.maxWeight ?? -1}, "${field(entry.provenance.sourcePath)}", "${field(entry.provenance.sourceSymbol)}", "${field(entry.provenance.sourceHash)}"}`
).join(',\n');

const header = `// Generated from pinned canonical PokéRogue content. Do not edit by hand.\n#pragma once\n#include <cstddef>\n#include <cstdint>\nnamespace PokerogueContent {\ninline constexpr char kContentHash[] = "${report.contentHash}";\ninline constexpr char kPokerogueRevision[] = "${report.sourceRevisions?.pokerogue ?? ''}";\ninline constexpr char kAssetsRevision[] = "${report.sourceRevisions?.assets ?? ''}";\ninline constexpr char kLocalesRevision[] = "${report.sourceRevisions?.locales ?? ''}";\nstruct Entity { const char* id; const char* name; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct AbilityMovegenProfile { uint16_t abilityId; uint8_t flags; double accuracyMultiplier; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nenum AbilityMovegenFlags : uint8_t { AbilityMovegenUnsupported = 1, AbilityMovegenMaxMultiHit = 2, AbilityMovegenInstantCharge = 4 };\nstruct Form { const char* id; const char* speciesId; const char* formKey; const char* atlasKey; const char* name; uint8_t hp; uint8_t atk; uint8_t def; uint8_t spatk; uint8_t spdef; uint8_t speed; uint16_t ability1; uint16_t ability2; uint16_t abilityHidden; const char* type1; const char* type2; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct Species { uint16_t dex; uint16_t malePercentTenths; uint8_t generation; int8_t starterCost; bool starterEligible; uint16_t baseTotal; uint8_t hp; uint8_t atk; uint8_t def; uint8_t spatk; uint8_t spdef; uint8_t speed; uint16_t ability1; uint16_t ability2; uint16_t abilityHidden; uint16_t abilityPassive; uint32_t learnsetOffset; uint16_t learnsetCount; int8_t legendary; int8_t subLegendary; int8_t mythical; const char* growthRate; const char* id; const char* name; const char* type1; const char* type2; const char* firstFormId; const char* assetSourcePath; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nenum MoveCategory : uint8_t { MovePhysical = 0, MoveSpecial = 1, MoveStatus = 2 };\nstruct MoveAttribute { const char* id; };\nstruct Move { uint16_t id; uint8_t category; int16_t power; int16_t accuracy; int16_t pp; int8_t priority; int16_t upstreamChance; uint8_t generation; uint16_t upstreamFlags; uint8_t multiHitType; uint32_t attributeOffset; uint16_t attributeCount; const char* key; const char* name; const char* type; const char* target; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct SpeciesLevelMove { uint16_t speciesDex; uint8_t level; uint16_t moveId; };\nstruct SpeciesEvolution { const char* sourceSpeciesId; const char* targetSpeciesId; uint16_t level; int16_t strongThreshold; int16_t normalThreshold; int16_t wildThreshold; uint16_t sourceOrder; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct BiomeEncounterPoolEntry { const char* biomeId; const char* tier; const char* timeOfDay; uint16_t memberIndex; const char* speciesId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct BiomeTrainerPoolEntry { const char* biomeId; const char* tier; const char* trainerId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr Species kSpecies[] = {\n${speciesRows}\n};\ninline constexpr SpeciesEvolution kSpeciesEvolutions[] = {\n${evolutionRows}\n};\ninline constexpr Form kForms[] = {\n${forms}\n};\ninline constexpr Move kMoves[] = {\n${moves}\n};\ninline constexpr MoveAttribute kMoveAttributes[] = {\n${moveAttributes.map(attribute => `    {"${field(attribute)}"}`).join(',\n')}\n};\ninline constexpr SpeciesLevelMove kSpeciesLevelMoves[] = {\n${levelMoves.map(move => `    {${move.dex}, ${move.level}, ${move.moveId}}`).join(',\n')}\n};\ninline constexpr Entity kAbilities[] = {\n${abilities}\n};\ninline constexpr AbilityMovegenProfile kAbilityMovegenProfiles[] = {\n${abilityMovegenProfileRows}\n};\ninline constexpr Entity kItems[] = {\n${items}\n};\ninline constexpr Entity kModes[] = {\n${modes}\n};\ninline constexpr Entity kBiomes[] = {\n${biomes}\n};\ninline constexpr BiomeEncounterPoolEntry kBiomeEncounterPools[] = {\n${encounterPoolRows}\n};\ninline constexpr BiomeTrainerPoolEntry kBiomeTrainerPools[] = {\n${trainerPoolRows}\n};\ninline constexpr Entity kRoutes[] = {\n${routes}\n};\ninline constexpr Entity kLocales[] = {\n${locales}\n};\ninline constexpr std::size_t kSpeciesCount = sizeof(kSpecies) / sizeof(kSpecies[0]);\ninline constexpr std::size_t kSpeciesEvolutionCount = sizeof(kSpeciesEvolutions) / sizeof(kSpeciesEvolutions[0]);\ninline constexpr std::size_t kFormCount = sizeof(kForms) / sizeof(kForms[0]);\ninline constexpr std::size_t kMoveCount = sizeof(kMoves) / sizeof(kMoves[0]); inline constexpr std::size_t kMoveAttributeCount = sizeof(kMoveAttributes) / sizeof(kMoveAttributes[0]);\ninline constexpr std::size_t kSpeciesLevelMoveCount = sizeof(kSpeciesLevelMoves) / sizeof(kSpeciesLevelMoves[0]);\ninline constexpr std::size_t kAbilityCount = sizeof(kAbilities) / sizeof(kAbilities[0]);\ninline constexpr std::size_t kAbilityMovegenProfileCount = sizeof(kAbilityMovegenProfiles) / sizeof(kAbilityMovegenProfiles[0]);\ninline constexpr std::size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);\ninline constexpr std::size_t kModeCount = sizeof(kModes) / sizeof(kModes[0]);\ninline constexpr std::size_t kBiomeCount = sizeof(kBiomes) / sizeof(kBiomes[0]);\ninline constexpr std::size_t kBiomeEncounterPoolCount = sizeof(kBiomeEncounterPools) / sizeof(kBiomeEncounterPools[0]);\ninline constexpr std::size_t kBiomeTrainerPoolCount = sizeof(kBiomeTrainerPools) / sizeof(kBiomeTrainerPools[0]);\ninline constexpr std::size_t kRouteCount = sizeof(kRoutes) / sizeof(kRoutes[0]);\ninline constexpr std::size_t kLocaleCount = sizeof(kLocales) / sizeof(kLocales[0]);\ninline constexpr const Species* findSpeciesByDex(uint16_t dex) { for (const auto& species : kSpecies) if (species.dex == dex) return &species; return nullptr; }\ninline constexpr const Form* findFormById(const char* id) { if (!id) return nullptr; for (const auto& form : kForms) { const char* a = form.id; const char* b = id; while (*a && *b && *a == *b) { ++a; ++b; } if (*a == *b) return &form; } return nullptr; }\ninline constexpr const Move* findMoveById(uint16_t id) { for (const auto& move : kMoves) if (move.id == id) return &move; return nullptr; } inline constexpr bool moveHasAttribute(const Move& move, const char* id) { if (!id || move.attributeOffset > kMoveAttributeCount || move.attributeCount > kMoveAttributeCount - move.attributeOffset) return false; for (uint32_t i = 0; i < move.attributeCount; ++i) { const char* a = kMoveAttributes[move.attributeOffset + i].id; const char* b = id; while (*a && *b && *a == *b) { ++a; ++b; } if (*a == *b) return true; } return false; }\ninline constexpr const AbilityMovegenProfile* findAbilityMovegenProfile(uint16_t id) { for (const auto& profile : kAbilityMovegenProfiles) if (profile.abilityId == id) return &profile; return nullptr; }\ninline constexpr const SpeciesLevelMove* levelMovesFor(const Species& species) { return species.learnsetCount ? &kSpeciesLevelMoves[species.learnsetOffset] : nullptr; }\nstatic_assert(findSpeciesByDex(6)->malePercentTenths == 875, "Charizard canonical gender ratio mismatch");\nstatic_assert(findSpeciesByDex(81)->malePercentTenths == 65534, "Magnemite genderless canonical data mismatch");\nstatic_assert(findSpeciesByDex(1)->ability2 == 65, "Bulbasaur normalized secondary ability mismatch");\nstatic_assert(findSpeciesByDex(16)->ability2 != findSpeciesByDex(16)->ability1, "Pidgey dual ability data mismatch");\nstatic_assert(findSpeciesByDex(1)->hp == 45 && findSpeciesByDex(1)->atk == 49 && findSpeciesByDex(1)->ability1 == 65, "Bulbasaur canonical battle data mismatch");\nstatic_assert(findMoveById(33)->power == 40 && findMoveById(33)->priority == 0, "Tackle canonical move data mismatch");\nstatic_assert(findMoveById(2)->attributeCount == 1 && moveHasAttribute(*findMoveById(2), "HighCritAttr"), "Karate Chop upstream move attribute mismatch");\nstatic_assert(findMoveById(33)->attributeCount == 0, "Tackle unexpected upstream move attributes");\nstatic_assert(levelMovesFor(*findSpeciesByDex(1))[0].moveId == 33, "Bulbasaur canonical level-up moves mismatch");\ninline constexpr char kStartingBiomeId[] = "${field(content.extensions?.upstreamStartingBiome?.id ?? 'plains')}";\n}\n`;
const expandedHeader = header
  .replace('inline constexpr Entity kItems[] = {', `struct ModifierPoolEntry { const char* pool; const char* tier; uint16_t slot; const char* itemId; double staticWeight; double maxWeight; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr ModifierPoolEntry kModifierPoolEntries[] = {\n${modifierPoolRows}\n};\ninline constexpr Entity kItems[] = {`)
  .replace('inline constexpr std::size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);', 'inline constexpr std::size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]); inline constexpr std::size_t kModifierPoolEntryCount = sizeof(kModifierPoolEntries) / sizeof(kModifierPoolEntries[0]);')
  .replace(
    'struct Entity { const char* id; const char* name; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };',
    'struct Entity { const char* id; const char* name; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; }; struct TrainerType { uint16_t id; uint16_t derivedTypeId; uint32_t moneyMultiplierMilli; uint8_t flags; uint32_t partyTemplateOffset; uint16_t partyTemplateCount; uint32_t callbackTemplateOffset; uint16_t callbackTemplateCount; uint32_t signatureOffset; uint8_t signatureCount; uint32_t speciesPoolOffset; uint16_t speciesPoolCount; const char* key; const char* name; const char* partyTemplateStatus; const char* configStatus; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; const char* configSourcePath; const char* configSourceSymbol; const char* configSourceHash; }; struct TrainerPartyTemplateRef { uint16_t trainerId; const char* templateKey; uint16_t maxWave; }; struct TrainerSignatureSpecies { const char* speciesId; }; struct TrainerSignatureChoice { uint16_t trainerId; uint8_t slot; bool isGroup; uint32_t speciesOffset; uint8_t speciesCount; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; }; struct TrainerPoolSpecies { const char* speciesId; }; struct TrainerPoolChoice { uint16_t memberIndex; uint32_t speciesOffset; uint8_t speciesCount; bool isGroup; }; struct TrainerSpeciesPool { uint16_t trainerId; uint8_t tierId; uint32_t candidateOffset; uint16_t candidateCount; const char* tier; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };'
  )
  .replace(
    'inline constexpr Species kSpecies[] = {',
    `enum GrowthRate : uint8_t { GrowthErratic = 0, GrowthFast, GrowthMediumFast, GrowthMediumSlow, GrowthSlow, GrowthFluctuating };\ninline constexpr uint32_t kExperienceLevels[6][100] = {\n${experienceRows}\n};\ninline constexpr Species kSpecies[] = {`
  )
  .replace(
    'struct BiomeTrainerPoolEntry { const char* biomeId; const char* tier; const char* trainerId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };',
    'struct BiomeTrainerPoolEntry { const char* biomeId; const char* tier; const char* trainerId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct BiomeTrainerChance { const char* biomeId; uint16_t denominator; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct Route { const char* id; const char* from; const char* to; uint16_t weight; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };'
  )
  .replace(
    'inline constexpr BiomeTrainerPoolEntry kBiomeTrainerPools[] = {',
    `inline constexpr BiomeTrainerChance kBiomeTrainerChances[] = {\n${biomeTrainerChanceRows}\n};\ninline constexpr BiomeTrainerPoolEntry kBiomeTrainerPools[] = {`
  )
  .replace(
    'inline constexpr std::size_t kBiomeTrainerPoolCount = sizeof(kBiomeTrainerPools) / sizeof(kBiomeTrainerPools[0]);',
    'inline constexpr std::size_t kBiomeTrainerPoolCount = sizeof(kBiomeTrainerPools) / sizeof(kBiomeTrainerPools[0]); inline constexpr std::size_t kBiomeTrainerChanceCount = sizeof(kBiomeTrainerChances) / sizeof(kBiomeTrainerChances[0]); inline constexpr const BiomeTrainerChance* findBiomeTrainerChance(const char* id) { if (!id) return nullptr; for (const auto& chance : kBiomeTrainerChances) { const char* a = chance.biomeId; const char* b = id; while (*a && *b && *a == *b) { ++a; ++b; } if (*a == *b) return &chance; } return nullptr; }'
  )
  .replace('inline constexpr Entity kRoutes[] = {', 'inline constexpr Route kRoutes[] = {')
  .replace(
    'inline constexpr const Species* findSpeciesByDex(uint16_t dex) { for (const auto& species : kSpecies) if (species.dex == dex) return &species; return nullptr; }',
    'inline constexpr const Species* findSpeciesByDex(uint16_t dex) { for (const auto& species : kSpecies) if (species.dex == dex) return &species; return nullptr; } inline constexpr const Route* routesFrom(const char* biomeId, std::size_t ordinal = 0) { if (!biomeId) return nullptr; for (const auto& route : kRoutes) { const char* a = route.from; const char* b = biomeId; while (*a && *b && *a == *b) { ++a; ++b; } if (*a == *b) { if (!ordinal) return &route; --ordinal; } } return nullptr; }'
  )
  .replace('struct SpeciesLevelMove { uint16_t speciesDex; uint8_t level; uint16_t moveId; };', 'struct SpeciesLevelMove { uint16_t speciesDex; uint8_t level; uint16_t moveId; }; struct SpeciesEggMove { uint16_t speciesDex; uint16_t moveId; };')
  .replace('inline constexpr Entity kAbilities[] = {', `inline constexpr SpeciesEggMove kSpeciesEggMoves[] = {\n${eggMoves.map(move => `    {${move.dex}, ${move.moveId}}`).join(',\n')}\n};\ninline constexpr Entity kAbilities[] = {`)
  .replace('inline constexpr Entity kItems[] = {', `inline constexpr TrainerSignatureSpecies kTrainerSignatureSpecies[] = {\n${trainerSignatureSpeciesRows}\n};\ninline constexpr TrainerSignatureChoice kTrainerSignatureChoices[] = {\n${trainerSignatureChoiceRows}\n};\ninline constexpr TrainerPoolSpecies kTrainerPoolSpecies[] = {\n${trainerPoolSpeciesRows}\n};\ninline constexpr TrainerPoolChoice kTrainerPoolChoices[] = {\n${trainerPoolChoiceRows}\n};\ninline constexpr TrainerSpeciesPool kTrainerSpeciesPools[] = {\n${trainerSpeciesPoolRows}\n};\ninline constexpr TrainerPartyTemplateRef kTrainerPartyTemplateRefs[] = {\n${trainerPartyTemplateRefRows}\n};\ninline constexpr TrainerType kTrainerTypes[] = {\n${trainerTypeRows}\n};\ninline constexpr Entity kItems[] = {`)
  .replace('inline constexpr std::size_t kAbilityCount = sizeof(kAbilities) / sizeof(kAbilities[0]);', 'inline constexpr std::size_t kAbilityCount = sizeof(kAbilities) / sizeof(kAbilities[0]); inline constexpr std::size_t kTrainerSignatureChoiceCount = sizeof(kTrainerSignatureChoices) / sizeof(kTrainerSignatureChoices[0]); inline constexpr std::size_t kTrainerSignatureSpeciesCount = sizeof(kTrainerSignatureSpecies) / sizeof(kTrainerSignatureSpecies[0]); inline constexpr std::size_t kTrainerTypeCount = sizeof(kTrainerTypes) / sizeof(kTrainerTypes[0]); inline constexpr std::size_t kTrainerPartyTemplateRefCount = sizeof(kTrainerPartyTemplateRefs) / sizeof(kTrainerPartyTemplateRefs[0]); inline constexpr std::size_t kTrainerSpeciesPoolCount = sizeof(kTrainerSpeciesPools) / sizeof(kTrainerSpeciesPools[0]); inline constexpr std::size_t kTrainerPoolChoiceCount = sizeof(kTrainerPoolChoices) / sizeof(kTrainerPoolChoices[0]); inline constexpr std::size_t kTrainerPoolSpeciesCount = sizeof(kTrainerPoolSpecies) / sizeof(kTrainerPoolSpecies[0]);')
  .replace('inline constexpr const Species* findSpeciesByDex(uint16_t dex)', 'inline constexpr const TrainerType* findTrainerType(uint16_t id) { for (const auto& trainer : kTrainerTypes) if (trainer.id == id) return &trainer; return nullptr; } inline constexpr const TrainerType* findTrainerTypeByKey(const char* key) { if (!key) return nullptr; for (const auto& trainer : kTrainerTypes) { const char* a = trainer.key; const char* b = key; while (*a && *b && *a == *b) { ++a; ++b; } if (*a == *b) return &trainer; } return nullptr; } inline constexpr const Species* findSpeciesByDex(uint16_t dex)')
  .replace('inline constexpr std::size_t kSpeciesLevelMoveCount = sizeof(kSpeciesLevelMoves) / sizeof(kSpeciesLevelMoves[0]);', 'inline constexpr std::size_t kSpeciesLevelMoveCount = sizeof(kSpeciesLevelMoves) / sizeof(kSpeciesLevelMoves[0]); inline constexpr std::size_t kSpeciesEggMoveCount = sizeof(kSpeciesEggMoves) / sizeof(kSpeciesEggMoves[0]);')
  .replace('struct BiomeTrainerPoolEntry { const char* biomeId; const char* tier; const char* trainerId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };', 'struct BiomeTrainerPoolEntry { const char* biomeId; const char* tier; uint16_t memberIndex; const char* trainerId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; }; struct TrainerPartySegment { uint8_t size; uint8_t strengthId; bool sameSpecies; bool balanced; uint8_t evolutionThresholdKindId; const char* strength; const char* evolutionThresholdKind; }; struct TrainerPartyTemplate { const char* key; uint8_t totalSize; bool isCompound; uint32_t segmentOffset; uint16_t segmentCount; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };')
  .replace('inline constexpr BiomeTrainerPoolEntry kBiomeTrainerPools[] = {', `inline constexpr TrainerPartySegment kTrainerPartySegments[] = {\n${trainerPartySegmentRows}\n};\ninline constexpr TrainerPartyTemplate kTrainerPartyTemplates[] = {\n${trainerPartyTableRows}\n};\ninline constexpr BiomeTrainerPoolEntry kBiomeTrainerPools[] = {`)
  .replace('inline constexpr std::size_t kBiomeTrainerPoolCount = sizeof(kBiomeTrainerPools) / sizeof(kBiomeTrainerPools[0]);', 'inline constexpr std::size_t kBiomeTrainerPoolCount = sizeof(kBiomeTrainerPools) / sizeof(kBiomeTrainerPools[0]); inline constexpr std::size_t kTrainerPartySegmentCount = sizeof(kTrainerPartySegments) / sizeof(kTrainerPartySegments[0]); inline constexpr std::size_t kTrainerPartyTemplateCount = sizeof(kTrainerPartyTemplates) / sizeof(kTrainerPartyTemplates[0]);')
  .replace('inline constexpr const Species* findSpeciesByDex(uint16_t dex)', 'inline constexpr const TrainerPartyTemplate* findTrainerPartyTemplate(const char* key) { if (!key) return nullptr; for (const auto& item : kTrainerPartyTemplates) { const char* a = item.key; const char* b = key; while (*a && *b && *a == *b) { ++a; ++b; } if (*a == *b) return &item; } return nullptr; } inline constexpr const TrainerPartySegment* trainerPartySegmentsFor(const TrainerPartyTemplate& item) { return item.segmentCount ? &kTrainerPartySegments[item.segmentOffset] : nullptr; } inline constexpr const TrainerPartyTemplateRef* trainerPartyTemplateRefsFor(const TrainerType& trainer) { return trainer.partyTemplateCount ? &kTrainerPartyTemplateRefs[trainer.partyTemplateOffset] : nullptr; } inline constexpr const TrainerSpeciesPool* trainerSpeciesPoolsFor(const TrainerType& trainer) { return trainer.speciesPoolCount ? &kTrainerSpeciesPools[trainer.speciesPoolOffset] : nullptr; } inline constexpr const TrainerSpeciesPool* trainerSpeciesPoolForTier(const TrainerType& trainer, const char* tier) { const auto* pools = trainerSpeciesPoolsFor(trainer); if (!pools || !tier) return nullptr; for (uint16_t i = 0; i < trainer.speciesPoolCount; ++i) { const char* a = pools[i].tier; const char* b = tier; while (*a && *b && *a == *b) { ++a; ++b; } if (*a == *b) return &pools[i]; } return nullptr; } inline constexpr const TrainerPoolChoice* trainerPoolChoicesFor(const TrainerSpeciesPool& pool) { return pool.candidateCount ? &kTrainerPoolChoices[pool.candidateOffset] : nullptr; } inline constexpr const TrainerPoolSpecies* trainerPoolSpeciesFor(const TrainerPoolChoice& choice) { return choice.speciesCount ? &kTrainerPoolSpecies[choice.speciesOffset] : nullptr; } inline constexpr const Species* findSpeciesByDex(uint16_t dex)');
const runtimeHeader = expandedHeader
  .replace('bool starterEligible; uint16_t baseTotal;', 'bool starterEligible; bool freshProfileStarter; uint8_t freshProfileStarterOrdinal; uint16_t baseTotal;')
  .replace('uint32_t learnsetOffset; uint16_t learnsetCount; int8_t legendary;', 'uint32_t learnsetOffset; uint16_t learnsetCount; uint32_t eggMoveOffset; uint16_t eggMoveCount; int8_t legendary;')
  .replace(
    'struct SpeciesEvolution { const char* sourceSpeciesId; const char* targetSpeciesId; uint16_t level; int16_t strongThreshold; int16_t normalThreshold; int16_t wildThreshold; uint16_t sourceOrder; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };',
    'struct SpeciesEvolution { const char* sourceSpeciesId; const char* targetSpeciesId; uint16_t level; int16_t strongThreshold; int16_t normalThreshold; int16_t wildThreshold; uint16_t sourceOrder; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; const char* preFormKey; const char* evoFormKey; };'
  )
  .replace(
    'struct Species { uint16_t dex;',
    'struct Species { uint16_t dex; uint16_t prevolutionDex; uint16_t baseExp;'
  )
  .replace(
    'struct Form { const char* id; const char* speciesId; const char* formKey; const char* atlasKey; const char* name; uint8_t hp; uint8_t atk; uint8_t def; uint8_t spatk; uint8_t spdef; uint8_t speed; uint16_t ability1; uint16_t ability2; uint16_t abilityHidden; const char* type1;',
    'struct Form { const char* id; const char* speciesId; const char* formKey; const char* atlasKey; const char* name; uint8_t hp; uint8_t atk; uint8_t def; uint8_t spatk; uint8_t spdef; uint8_t speed; uint16_t ability1; uint16_t ability2; uint16_t abilityHidden; uint32_t learnsetOffset; uint16_t learnsetCount; const char* type1;'
  )
  .replace(
    'enum MoveCategory : uint8_t { MovePhysical = 0, MoveSpecial = 1, MoveStatus = 2 };',
    'enum MoveCategory : uint8_t { MovePhysical = 0, MoveSpecial = 1, MoveStatus = 2 }; enum MoveUpstreamFlags : uint16_t { MoveIsUnimplemented = 1, MoveHasSacrificialAttrOnHit = 2, MoveHasMultiHit = 4, MoveHasMultiHitPowerIncrement = 8, MoveHasDelayedAttack = 16, MoveHasRecharge = 32, MoveIsCharging = 64, MoveChecksAccuracyPerHit = 128, MoveUsesDefense = 256, MoveSelectsOffensiveCategory = 512, MoveHasSacrificialAttr = 1024, MoveIsStabBlacklisted = 2048 };'
  )
  .replace(
    'struct SpeciesLevelMove { uint16_t speciesDex; uint8_t level; uint16_t moveId; };',
    'struct SpeciesLevelMove { uint16_t speciesDex; int8_t level; uint16_t moveId; };'
  )
  .replace(
    'inline constexpr const SpeciesLevelMove* levelMovesFor(const Species& species) { return species.learnsetCount ? &kSpeciesLevelMoves[species.learnsetOffset] : nullptr; }',
    'inline constexpr const SpeciesLevelMove* levelMovesFor(const Species& species) { return species.learnsetCount ? &kSpeciesLevelMoves[species.learnsetOffset] : nullptr; } inline constexpr const SpeciesLevelMove* levelMovesFor(const Form& form) { return form.learnsetCount ? &kSpeciesLevelMoves[form.learnsetOffset] : nullptr; } inline constexpr const SpeciesEggMove* eggMovesFor(const Species& species) { return species.eggMoveCount ? &kSpeciesEggMoves[species.eggMoveOffset] : nullptr; }'
  )
  .replace(
    'inline constexpr const Species* findSpeciesByDex(uint16_t dex)',
    `inline constexpr uint16_t kClassicFinalWave = ${classicFinalWave.wave};\ninline constexpr char kClassicFinalWaveSourcePath[] = "${field(classicFinalWaveProvenance.sourcePath)}";\ninline constexpr char kClassicFinalWaveSourceSymbol[] = "${field(classicFinalWaveProvenance.sourceSymbol)}";\ninline constexpr char kClassicFinalWaveSourceHash[] = "${field(classicFinalWaveProvenance.sourceHash)}";\nstruct ClassicFixedBossWave { uint16_t wave; const char* upstreamSymbol; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr ClassicFixedBossWave kClassicFixedBossWaves[] = {\n${fixedBossWaveRows}\n};\ninline constexpr std::size_t kClassicFixedBossWaveCount = sizeof(kClassicFixedBossWaves) / sizeof(kClassicFixedBossWaves[0]);\ninline constexpr const ClassicFixedBossWave* findClassicFixedBossWave(uint16_t wave) { for (const auto& entry : kClassicFixedBossWaves) if (entry.wave == wave) return &entry; return nullptr; }\nstruct ClassicFixedBattleWave { uint16_t wave; uint16_t trainerTypeId; bool hasStaticTrainerType; bool seededBinaryGenderVariant; const char* upstreamSymbol; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr ClassicFixedBattleWave kClassicFixedBattleWaves[] = {\n${fixedBattleWaveRows}\n};\ninline constexpr std::size_t kClassicFixedBattleWaveCount = sizeof(kClassicFixedBattleWaves) / sizeof(kClassicFixedBattleWaves[0]);\ninline constexpr const ClassicFixedBattleWave* findClassicFixedBattleWave(uint16_t wave) { for (const auto& entry : kClassicFixedBattleWaves) if (entry.wave == wave) return &entry; return nullptr; }\ninline constexpr bool isClassicMajorBossWave(uint16_t wave) { return wave && wave % 10 == 0; }\ninline constexpr const Species* findSpeciesByDex(uint16_t dex)`
  );
const atlasHeader = runtimeHeader
  .replace('struct SpeciesLevelMove {', 'struct PokemonSpriteAtlas { uint16_t speciesDex; uint16_t width; uint16_t height; uint32_t frameOffset; uint16_t frameCount; const char* manifestPath; const char* imagePath; const char* manifestHash; }; struct PokemonSpriteFrame { const char* filename; uint16_t x; uint16_t y; uint16_t width; uint16_t height; uint16_t sourceWidth; uint16_t sourceHeight; uint16_t trimX; uint16_t trimY; }; struct SpeciesLevelMove {')
  .replace('inline constexpr Entity kLocales[] = {', `inline constexpr PokemonSpriteFrame kPokemonSpriteFrames[] = {\n${spriteFrameRows}\n};\ninline constexpr PokemonSpriteAtlas kPokemonSpriteAtlases[] = {\n${spriteAtlasRows}\n};\ninline constexpr Entity kLocales[] = {`)
  .replace('inline constexpr std::size_t kLocaleCount = sizeof(kLocales) / sizeof(kLocales[0]);', 'inline constexpr std::size_t kLocaleCount = sizeof(kLocales) / sizeof(kLocales[0]); inline constexpr std::size_t kPokemonSpriteFrameCount = sizeof(kPokemonSpriteFrames) / sizeof(kPokemonSpriteFrames[0]); inline constexpr std::size_t kPokemonSpriteAtlasCount = sizeof(kPokemonSpriteAtlases) / sizeof(kPokemonSpriteAtlases[0]); inline constexpr const PokemonSpriteAtlas* findPokemonSpriteAtlas(uint16_t dex) { for (const auto& atlas : kPokemonSpriteAtlases) if (atlas.speciesDex == dex) return &atlas; return nullptr; }');
await fs.mkdir(path.dirname(outputPath), { recursive: true });
await fs.writeFile(outputPath, atlasHeader, 'utf8');
console.log(JSON.stringify({ output: path.relative(root, outputPath), bytes: Buffer.byteLength(atlasHeader), hash: report.contentHash }));
