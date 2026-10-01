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
const supercedence = content.extensions?.trainerMoveSupercedence;
if (!Array.isArray(supercedence?.pairs) || !supercedence.pairs.length ||
    supercedence.provenance?.sourcePath !== 'src/data/balance/moves/superceded-moves.ts' ||
    supercedence.provenance?.sourceSymbol !== 'SUPERCEDED_MOVES' ||
    !supercedence.provenance?.sourceHash) {
  throw new Error('Pinned trainer move supercedence or provenance is missing');
}
const catalogMoveIds = new Set(collections.moves.map(move => move.moveId));
for (const pair of supercedence.pairs) {
  if (!catalogMoveIds.has(pair.moveId) || !catalogMoveIds.has(pair.replacementMoveId))
    throw new Error(`Trainer move supercedence references a missing canonical move: ${JSON.stringify(pair)}`);
}
const supercedenceRows = supercedence.pairs.map(pair =>
  `    {${pair.moveId}, ${pair.replacementMoveId}}`).join(',\n');
const moveBlocklists = content.extensions?.trainerMoveBlocklists;
if (moveBlocklists?.provenance?.sourcePath !== 'src/data/balance/moves/forbidden-moves.ts' ||
    !moveBlocklists.provenance.sourceHash ||
    ['singles', 'levelBased', 'tm'].some(key => !Array.isArray(moveBlocklists[key]) ||
      !moveBlocklists[key].length || moveBlocklists[key].some(id => !catalogMoveIds.has(id)) ||
      new Set(moveBlocklists[key]).size !== moveBlocklists[key].length)) {
  throw new Error('Pinned trainer move blocklists are missing or invalid');
}
const moveBlocklistRows = key => moveBlocklists[key].map(id => `    ${id}`).join(',\n');
const forcedSignatures = content.extensions?.forcedSignatureMoves;
const canonicalSpeciesDex = new Set(collections.species.map(species => species.speciesId));
if (forcedSignatures?.provenance?.sourcePath !== 'src/data/balance/moves/signature-moves.ts' ||
    !forcedSignatures.provenance.sourceHash ||
    !Number.isInteger(forcedSignatures.chancePercent) ||
    forcedSignatures.chancePercent < 1 || forcedSignatures.chancePercent > 100 ||
    forcedSignatures.chanceProvenance?.sourcePath !== 'src/data/balance/moves/moveset-generation.ts' ||
    !forcedSignatures.chanceProvenance.sourceHash ||
    ['regular', 'rival'].some(key => !Array.isArray(forcedSignatures[key]) ||
      !forcedSignatures[key].length || forcedSignatures[key].some(entry =>
        !canonicalSpeciesDex.has(entry.speciesDex) || typeof entry.isArray !== 'boolean' ||
        !Array.isArray(entry.moveIds) || !entry.moveIds.length ||
        entry.moveIds.some(id => !catalogMoveIds.has(id))))) {
  throw new Error('Pinned forced signature move map is missing or invalid');
}
const signatureRows = ['regular', 'rival'].flatMap(key => forcedSignatures[key].flatMap(entry =>
  entry.moveIds.map(moveId => `    {${entry.speciesDex}, ${moveId}, ${entry.isArray}, ${key === 'rival'}}`))).join(',\n');
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
  // TrainerPartyCompoundTemplate calls super(size, AVERAGE), retaining NORMAL.
  const normal = template.segments.find(segment => segment.evolutionThresholdKind === 'NORMAL')
    ?? collections.trainerPartyTemplates.flatMap(row => row.segments).find(segment => segment.evolutionThresholdKind === 'NORMAL');
  if (!normal) throw new Error('Canonical NORMAL trainer evolution threshold is missing');
  const parentEvolutionThresholdKindId = template.isCompound ? normal.evolutionThresholdKindId : template.segments[0].evolutionThresholdKindId;
  return { template, source, offset, count: trainerPartySegments.length - offset, parentEvolutionThresholdKindId };
});
if (trainerPartySegments.length > 0xFFFFFFFF) throw new Error('Trainer party segment table exceeds 32-bit offset range');
const trainerPartySegmentRows = trainerPartySegments.map(segment =>
  `    {${segment.size}, ${segment.strengthId}, ${segment.sameSpecies ? 'true' : 'false'}, ${segment.balanced ? 'true' : 'false'}, ${segment.evolutionThresholdKindId}, "${field(segment.strength)}", "${field(segment.evolutionThresholdKind)}"}`
).join(',\n');
const trainerPartyTableRows = trainerPartyTemplateRows.map(({ template, source, offset, count, parentEvolutionThresholdKindId }) =>
  `    {"${field(template.templateKey)}", ${template.totalSize}, ${template.isCompound ? 'true' : 'false'}, ${offset}, ${count}, ${parentEvolutionThresholdKindId}, "${field(source.sourcePath)}", "${field(source.sourceSymbol)}", "${field(source.sourceHash)}"}`
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
  return `    {${trainer.trainerTypeId}, ${derivedType}, ${missingConfig ? 0 : Math.round(trainer.moneyMultiplier * 1000)}, ${flags}, ${partyOffset}, ${trainer.partyTemplateKeys.length}, ${callbackOffset}, ${callbackKeys.length}, ${signatureOffset}, ${signature.length}, ${poolOffset}, ${trainerPoolRowsNative.length - poolOffset}, "${field(trainer.id)}", "${field(trainer.name)}", "${field(trainer.trainerRules?.specialtyType ?? '')}", ${trainer.trainerRules?.specialtyTypeStatus === 'RESOLVED' ? 'true' : 'false'}, "${field(templateStatus)}", "${field(configStatus)}", "${field(source.sourcePath)}", "${field(source.sourceSymbol)}", "${field(source.sourceHash)}", "${field(configSource.sourcePath ?? '')}", "${field(configSource.sourceSymbol ?? '')}", "${field(configSource.sourceHash ?? '')}"}`;
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
const moveStatStageEffects = [];
const upstreamStatIds = { ATK: 1, DEF: 2, SPATK: 3, SPDEF: 4, SPD: 5, ACC: 6, EVA: 7 };
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
  const statStageDeclarations = [...upstreamRaw.matchAll(/\.attr\s*\(\s*StatStageChangeAttr\b/g)];
  const parsedStatStages = [...upstreamRaw.matchAll(/\.attr\s*\(\s*StatStageChangeAttr\s*,\s*\[[^\]]*\]\s*,\s*(-?\d+)(?:\s*,\s*(true|false))?/g)];
  if (statStageDeclarations.length !== parsedStatStages.length)
    throw new Error(`Unsupported trainer stat-stage weighting metadata: ${move.id}`);
  // Only complete constant constructors enter the native effect table.
  // Options/callbacks remain preserved in canonical raw metadata and unsupported.
  const constantStageEffects = [...upstreamRaw.matchAll(
    /\.attr\s*\(\s*StatStageChangeAttr\s*,\s*\[([^\]]*)\]\s*,\s*(-?\d+)\s*(?:,\s*(true|false)\s*)?\)/g
  )];
  if (constantStageEffects.length === statStageDeclarations.length) {
    const parsed = constantStageEffects.map(match => {
      const names = match[1].split(',').map(name => name.trim()).filter(Boolean);
      let statMask = 0;
      for (const name of names) {
        const id = upstreamStatIds[name.replace(/^Stat\./, '')];
        if (!/^Stat\.[A-Z]+$/.test(name) || !id) return null;
        statMask |= 1 << (id - 1);
      }
      const stages = Number(match[2]);
      if (!statMask || !Number.isInteger(stages) || stages < -6 || stages > 6) return null;
      return { statMask, stages, selfTarget: match[3] === 'true' };
    });
    if (parsed.every(Boolean)) for (const effect of parsed)
      moveStatStageEffects.push(`    {${move.moveId}, ${effect.statMask}, ${effect.stages}, ${effect.selfTarget}}`);
  }
  const strongSelfStatBoost = parsedStatStages.some(match => Number(match[1]) > 1 && match[2] === 'true');
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
  const selfStatus = /^\s*new\s+SelfStatusMove\s*\(/.test(upstreamRaw);
  const offensiveSelfBoost = selfStatus && sourceAttributes.length === 1 &&
    (['curse', 'bulk_up', 'hone_claws', 'calm_mind', 'take_heart'].includes(move.id) ||
      [...upstreamRaw.matchAll(/\.attr\s*\(\s*StatStageChangeAttr\s*,\s*\[([^\]]*)\]/g)]
        .some(match => /^\s*Stat\.(?:ATK|SPATK)\s*$/.test(match[1])));
  const requiresPostSelectionFilter = sourceAttributes.includes('WeatherChangeAttr') ||
    offensiveSelfBoost ||
    /targetSleptOrComatoseCondition|userSleptOrComatoseCondition/.test(upstreamRaw) ||
    ['rain_dance', 'sunny_day', 'snowscape', 'hail', 'sandstorm', 'aurora_veil',
      'solar_beam', 'solar_blade', 'venom_drench'].includes(move.id);
  const variableMovegenType = move.category !== 'Status' &&
    sourceAttributes.some(attribute => /TypeAttr$/.test(attribute));
  const upstreamFlags = [];
  if (move.isUnimplemented || /\.unimplemented\s*\(/.test(upstreamRaw)) upstreamFlags.push('MoveIsUnimplemented');
  if (/SacrificialAttrOnHit/.test(upstreamRaw)) upstreamFlags.push('MoveHasSacrificialAttrOnHit');
  if (/SacrificialAttr/.test(upstreamRaw)) upstreamFlags.push('MoveHasSacrificialAttr');
  if (move.extensions?.upstreamMoveGeneration?.stabBlacklisted === true) upstreamFlags.push('MoveIsStabBlacklisted');
  if (/\.attr\s*\(\s*MultiHitAttr/.test(upstreamRaw)) upstreamFlags.push('MoveHasMultiHit');
  if (/MultiHitPowerIncrementAttr/.test(upstreamRaw)) upstreamFlags.push('MoveHasMultiHitPowerIncrement');
  if (/DelayedAttackAttr/.test(upstreamRaw)) upstreamFlags.push('MoveHasDelayedAttack');
  if (/RechargeAttr/.test(upstreamRaw)) upstreamFlags.push('MoveHasRecharge');
  if (strongSelfStatBoost) upstreamFlags.push('MoveHasStrongSelfStatBoost');
  if (requiresPostSelectionFilter) upstreamFlags.push('MoveRequiresPostSelectionFilter');
  if (variableMovegenType) upstreamFlags.push('MoveHasVariableMovegenType');
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
const abilityStatStageRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const multiplierAttrs = [...raw.matchAll(/\.attr\s*\(\s*StatStageChangeMultiplierAbAttr\b/g)];
  const multipliers = [...raw.matchAll(/\.attr\s*\(\s*StatStageChangeMultiplierAbAttr\s*,\s*(-?\d+)\s*\)/g)];
  const protectionAttrs = [...raw.matchAll(/\.attr\s*\(\s*ProtectStatAbAttr\b/g)];
  const protections = [...raw.matchAll(/\.attr\s*\(\s*ProtectStatAbAttr\s*(?:,\s*Stat\.([A-Z]+)\s*)?\)/g)];
  const reflectionAttrs = [...raw.matchAll(/\.attr\s*\(\s*ReflectStatStageChangeAbAttr\b/g)];
  const reflections = [...raw.matchAll(/\.attr\s*\(\s*ReflectStatStageChangeAbAttr\s*\)/g)];
  const copyAttrs = [...raw.matchAll(/\.attr\s*\(\s*StatStageChangeCopyAbAttr\b/g)];
  const copies = [...raw.matchAll(/\.attr\s*\(\s*StatStageChangeCopyAbAttr\s*\)/g)];
  if (!multiplierAttrs.length && !protectionAttrs.length && !reflectionAttrs.length && !copyAttrs.length) return [];
  if (multiplierAttrs.length !== multipliers.length || protectionAttrs.length !== protections.length || reflectionAttrs.length !== reflections.length || copyAttrs.length !== copies.length) return [];
  let multiplier = 1, protectedMask = 0;
  for (const match of multipliers) multiplier *= Number(match[1]);
  if (!Number.isInteger(multiplier) || multiplier < -6 || multiplier > 6) return [];
  for (const match of protections) {
    if (!match[1]) protectedMask = 127;
    else if (upstreamStatIds[match[1]]) protectedMask |= 1 << (upstreamStatIds[match[1]] - 1);
    else return [];
  }
  return [`    {${ability.abilityId}, ${multiplier}, ${protectedMask}, ${reflections.length > 0}, ${copies.length > 0}, ${/\.ignorable\s*\(\s*\)/.test(raw)}, "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`];
});
const abilityStatReactionRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const match = raw.match(/\.attr\s*\(\s*PostStatStageChangeStatStageChangeAbAttr\s*,\s*\(\s*_target\s*,\s*changes\s*\)\s*=>\s*\(\s*\{\s*stat\s*:\s*Stat\.([A-Z]+)\s*,\s*stages\s*:\s*changes\[0\]\.stages\s*<\s*0\s*\?\s*(\d+)\s*\*\s*changes\.length\s*:\s*0\s*,?\s*\}\s*\)\s*\)/);
  if (!match || !upstreamStatIds[match[1]]) return [];
  const multiplier = Number(match[2]);
  if (!Number.isInteger(multiplier) || multiplier < 1 || multiplier > 6) return [];
  return [`    {${ability.abilityId}, ${upstreamStatIds[match[1]]}, ${multiplier}, "${field(ability.source?.sourceSymbol ?? '')}"}`];
});
const negativeStageResetItemRows = collections.items.filter(item =>
  /new\s+ResetNegativeStatStageModifier\s*\(/.test(item.extensions?.upstreamRawRecord?.value ?? '')
).map(item => `    {"${field(item.id)}", "${field(item.source?.sourcePath ?? '')}", "${field(item.source?.sourceSymbol ?? '')}", "${field(item.source?.sourceHash ?? '')}"}`);
const lowHpTypePowerRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*LowHpMoveTypePowerBoostAbAttr\b/g)];
  const matches = [...raw.matchAll(/\.attr\s*\(\s*LowHpMoveTypePowerBoostAbAttr\s*,\s*PokemonType\.([A-Z]+)\s*\)/g)];
  if (!declarations.length || declarations.length !== matches.length) return [];
  return matches.map(match => `    {${ability.abilityId}, "${field(match[1])}", "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`);
});
const typePowerAbilityRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*MoveTypePowerBoostAbAttr\b/g)];
  const matches = [...raw.matchAll(/\.attr\s*\(\s*MoveTypePowerBoostAbAttr\s*,\s*PokemonType\.([A-Z]+)\s*(?:,\s*(\d+(?:\.\d+)?)\s*(?:,\s*(?:true|false)\s*)?)?\)/g)];
  if (!declarations.length || declarations.length !== matches.length) return [];
  const requiresCondition = /\.condition\s*\(/.test(raw);
  const weatherCondition = raw.match(/\.condition\s*\(\s*getWeatherCondition\s*\(\s*WeatherType\.([A-Z_]+)\s*\)\s*\)/);
  const conditionCount = [...raw.matchAll(/\.condition\s*\(/g)].length;
  const conditionWeather = weatherCondition && conditionCount === 1 ? weatherCondition[1] : '';
  return matches.map(match => {
    const multiplier = match[2] == null || Number(match[2]) === 0 ? 1.5 : Number(match[2]);
    if (!Number.isFinite(multiplier) || multiplier <= 0 || multiplier > 16) throw new Error(`Invalid type-power ability: ${ability.id}`);
    return `    {${ability.abilityId}, "${field(match[1])}", ${multiplier}, ${requiresCondition}, "${field(conditionWeather)}", "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`;
  });
});
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
  if (aiOffsets.length > 1 && ability.id !== 'hustle') flags |= 1;
  if (aiOffset >= 0) {
    const bodyMatch = callback.match(/=>\s*\{([\s\S]*?)\}\s*\)\s*$/);
    const callbackBody = bodyMatch?.[1]?.trim() ?? '';
    const accuracy = callback.match(/accMult\.value\s*\*=\s*(\d+(?:\.\d+)?)/);
    const accuracyOnly = /^accMult\.value\s*\*=\s*\d+(?:\.\d+)?\s*;?\s*$/.test(callbackBody);
    const maxMultiHitOnly = /^maxMultiHit\.value\s*=\s*true\s*;?\s*$/.test(callbackBody);
    const normalizedBody = callbackBody.replace(/\s+/g, ' ').trim();
    const hustleMovegen = ability.id === 'hustle' && normalizedBody ===
      'if (move.category === MoveCategory.PHYSICAL) { accMult.value *= 0.8; powerMult.value *= 1.5; }';
    const analyticMovegen = ability.id === 'analytic' && normalizedBody ===
      'if (move.priority < 0) { powerMult.value *= 1.3; }';
    const recognized = (accuracyOnly && accuracy) || (maxMultiHitOnly && hasMaxMultiHit) ||
      hustleMovegen || analyticMovegen;
    if (recognized && accuracyOnly) accuracyMultiplier = Number(accuracy[1]);
    if (hustleMovegen) flags |= 8;
    if (analyticMovegen) flags |= 16;
    if (!recognized) flags |= 1;
  }
  if (!Number.isInteger(ability.abilityId) || ability.abilityId < 0 || ability.abilityId > 0xFFFF) throw new Error(`Invalid canonical ability ID: ${ability.id}`);
  // Damage callback capability is deliberately explicit: unknown attributes,
  // conditions and unimplemented builders cannot silently skip boss callbacks.
  const attributes = [...raw.matchAll(/\.(?:attr|conditionalAttr)\s*\(\s*([A-Za-z_$][A-Za-z0-9_$]*)/g)].map(match => match[1]);
  const attributeCalls = [...raw.matchAll(/\.(?:attr|conditionalAttr)\s*\(/g)].length;
  const builderCalls = [...raw.matchAll(/\.([A-Za-z_$][A-Za-z0-9_$]*)\s*\(/g)].map(match => match[1]);
  const knownBuilderCalls = new Set(['attr', 'build', 'uncopiable', 'unreplaceable', 'unsuppressable', 'ignorable']);
  const bossDamageCallbacksResolved = builderCalls.every(name => knownBuilderCalls.has(name)) && !!raw && /new AbBuilder\(/.test(raw) &&
    attributeCalls === attributes.length && !/\.(?:condition|conditionalAttr|unimplemented|partial)\s*\(/.test(raw) &&
    attributes.every(name => ['IncreasePpUsedAbAttr', 'NonSuperEffectiveImmunityAbAttr'].includes(name));
  return { abilityId: ability.abilityId, flags, accuracyMultiplier, bossDamageCallbacksResolved, source: ability.source };
}).sort((left, right) => left.abilityId - right.abilityId);
const abilityMovegenProfileRows = abilityMovegenProfiles.map(profile =>
  `    {${profile.abilityId}, ${profile.flags}, ${profile.accuracyMultiplier}, ${profile.bossDamageCallbacksResolved ? 'true' : 'false'}, "${field(profile.source?.sourcePath ?? '')}", "${field(profile.source?.sourceSymbol ?? '')}", "${field(profile.source?.sourceHash ?? '')}"}`
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

const header = `// Generated from pinned canonical PokéRogue content. Do not edit by hand.\n#pragma once\n#include <cstddef>\n#include <cstdint>\nnamespace PokerogueContent {\ninline constexpr char kContentHash[] = "${report.contentHash}";\ninline constexpr char kPokerogueRevision[] = "${report.sourceRevisions?.pokerogue ?? ''}";\ninline constexpr char kAssetsRevision[] = "${report.sourceRevisions?.assets ?? ''}";\ninline constexpr char kLocalesRevision[] = "${report.sourceRevisions?.locales ?? ''}";\nstruct Entity { const char* id; const char* name; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct AbilityMovegenProfile { uint16_t abilityId; uint8_t flags; double accuracyMultiplier; bool bossDamageCallbacksResolved; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nenum AbilityMovegenFlags : uint8_t { AbilityMovegenUnsupported = 1, AbilityMovegenMaxMultiHit = 2, AbilityMovegenInstantCharge = 4, AbilityMovegenHustle = 8, AbilityMovegenAnalytic = 16 };\nstruct Form { const char* id; const char* speciesId; const char* formKey; const char* atlasKey; const char* name; uint8_t hp; uint8_t atk; uint8_t def; uint8_t spatk; uint8_t spdef; uint8_t speed; uint16_t ability1; uint16_t ability2; uint16_t abilityHidden; const char* type1; const char* type2; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct Species { uint16_t dex; uint16_t malePercentTenths; uint8_t generation; int8_t starterCost; bool starterEligible; uint16_t baseTotal; uint8_t hp; uint8_t atk; uint8_t def; uint8_t spatk; uint8_t spdef; uint8_t speed; uint16_t ability1; uint16_t ability2; uint16_t abilityHidden; uint16_t abilityPassive; uint32_t learnsetOffset; uint16_t learnsetCount; int8_t legendary; int8_t subLegendary; int8_t mythical; const char* growthRate; const char* id; const char* name; const char* type1; const char* type2; const char* firstFormId; const char* assetSourcePath; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nenum MoveCategory : uint8_t { MovePhysical = 0, MoveSpecial = 1, MoveStatus = 2 };\nstruct MoveAttribute { const char* id; };\nstruct Move { uint16_t id; uint8_t category; int16_t power; int16_t accuracy; int16_t pp; int8_t priority; int16_t upstreamChance; uint8_t generation; uint16_t upstreamFlags; uint8_t multiHitType; uint32_t attributeOffset; uint16_t attributeCount; const char* key; const char* name; const char* type; const char* target; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct SpeciesLevelMove { uint16_t speciesDex; uint8_t level; uint16_t moveId; };\nstruct SpeciesEvolution { const char* sourceSpeciesId; const char* targetSpeciesId; uint16_t level; int16_t strongThreshold; int16_t normalThreshold; int16_t wildThreshold; uint16_t sourceOrder; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct BiomeEncounterPoolEntry { const char* biomeId; const char* tier; const char* timeOfDay; uint16_t memberIndex; const char* speciesId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct BiomeTrainerPoolEntry { const char* biomeId; const char* tier; const char* trainerId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr Species kSpecies[] = {\n${speciesRows}\n};\ninline constexpr SpeciesEvolution kSpeciesEvolutions[] = {\n${evolutionRows}\n};\ninline constexpr Form kForms[] = {\n${forms}\n};\ninline constexpr Move kMoves[] = {\n${moves}\n};\ninline constexpr MoveAttribute kMoveAttributes[] = {\n${moveAttributes.map(attribute => `    {"${field(attribute)}"}`).join(',\n')}\n};\ninline constexpr SpeciesLevelMove kSpeciesLevelMoves[] = {\n${levelMoves.map(move => `    {${move.dex}, ${move.level}, ${move.moveId}}`).join(',\n')}\n};\ninline constexpr Entity kAbilities[] = {\n${abilities}\n};\ninline constexpr AbilityMovegenProfile kAbilityMovegenProfiles[] = {\n${abilityMovegenProfileRows}\n};\ninline constexpr Entity kItems[] = {\n${items}\n};\ninline constexpr Entity kModes[] = {\n${modes}\n};\ninline constexpr Entity kBiomes[] = {\n${biomes}\n};\ninline constexpr BiomeEncounterPoolEntry kBiomeEncounterPools[] = {\n${encounterPoolRows}\n};\ninline constexpr BiomeTrainerPoolEntry kBiomeTrainerPools[] = {\n${trainerPoolRows}\n};\ninline constexpr Entity kRoutes[] = {\n${routes}\n};\ninline constexpr Entity kLocales[] = {\n${locales}\n};\ninline constexpr std::size_t kSpeciesCount = sizeof(kSpecies) / sizeof(kSpecies[0]);\ninline constexpr std::size_t kSpeciesEvolutionCount = sizeof(kSpeciesEvolutions) / sizeof(kSpeciesEvolutions[0]);\ninline constexpr std::size_t kFormCount = sizeof(kForms) / sizeof(kForms[0]);\ninline constexpr std::size_t kMoveCount = sizeof(kMoves) / sizeof(kMoves[0]); inline constexpr std::size_t kMoveAttributeCount = sizeof(kMoveAttributes) / sizeof(kMoveAttributes[0]);\ninline constexpr std::size_t kSpeciesLevelMoveCount = sizeof(kSpeciesLevelMoves) / sizeof(kSpeciesLevelMoves[0]);\ninline constexpr std::size_t kAbilityCount = sizeof(kAbilities) / sizeof(kAbilities[0]);\ninline constexpr std::size_t kAbilityMovegenProfileCount = sizeof(kAbilityMovegenProfiles) / sizeof(kAbilityMovegenProfiles[0]);\ninline constexpr std::size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);\ninline constexpr std::size_t kModeCount = sizeof(kModes) / sizeof(kModes[0]);\ninline constexpr std::size_t kBiomeCount = sizeof(kBiomes) / sizeof(kBiomes[0]);\ninline constexpr std::size_t kBiomeEncounterPoolCount = sizeof(kBiomeEncounterPools) / sizeof(kBiomeEncounterPools[0]);\ninline constexpr std::size_t kBiomeTrainerPoolCount = sizeof(kBiomeTrainerPools) / sizeof(kBiomeTrainerPools[0]);\ninline constexpr std::size_t kRouteCount = sizeof(kRoutes) / sizeof(kRoutes[0]);\ninline constexpr std::size_t kLocaleCount = sizeof(kLocales) / sizeof(kLocales[0]);\ninline constexpr const Species* findSpeciesByDex(uint16_t dex) { for (const auto& species : kSpecies) if (species.dex == dex) return &species; return nullptr; }\ninline constexpr const Form* findFormById(const char* id) { if (!id) return nullptr; for (const auto& form : kForms) { const char* a = form.id; const char* b = id; while (*a && *b && *a == *b) { ++a; ++b; } if (*a == *b) return &form; } return nullptr; }\ninline constexpr const Move* findMoveById(uint16_t id) { for (const auto& move : kMoves) if (move.id == id) return &move; return nullptr; } inline constexpr bool moveHasAttribute(const Move& move, const char* id) { if (!id || move.attributeOffset > kMoveAttributeCount || move.attributeCount > kMoveAttributeCount - move.attributeOffset) return false; for (uint32_t i = 0; i < move.attributeCount; ++i) { const char* a = kMoveAttributes[move.attributeOffset + i].id; const char* b = id; while (*a && *b && *a == *b) { ++a; ++b; } if (*a == *b) return true; } return false; }\ninline constexpr const AbilityMovegenProfile* findAbilityMovegenProfile(uint16_t id) { for (const auto& profile : kAbilityMovegenProfiles) if (profile.abilityId == id) return &profile; return nullptr; }\ninline constexpr const SpeciesLevelMove* levelMovesFor(const Species& species) { return species.learnsetCount ? &kSpeciesLevelMoves[species.learnsetOffset] : nullptr; }\nstatic_assert(findSpeciesByDex(6)->malePercentTenths == 875, "Charizard canonical gender ratio mismatch");\nstatic_assert(findSpeciesByDex(81)->malePercentTenths == 65534, "Magnemite genderless canonical data mismatch");\nstatic_assert(findSpeciesByDex(1)->ability2 == 65, "Bulbasaur normalized secondary ability mismatch");\nstatic_assert(findSpeciesByDex(16)->ability2 != findSpeciesByDex(16)->ability1, "Pidgey dual ability data mismatch");\nstatic_assert(findSpeciesByDex(1)->hp == 45 && findSpeciesByDex(1)->atk == 49 && findSpeciesByDex(1)->ability1 == 65, "Bulbasaur canonical battle data mismatch");\nstatic_assert(findMoveById(33)->power == 40 && findMoveById(33)->priority == 0, "Tackle canonical move data mismatch");\nstatic_assert(findMoveById(2)->attributeCount == 1 && moveHasAttribute(*findMoveById(2), "HighCritAttr"), "Karate Chop upstream move attribute mismatch");\nstatic_assert(findMoveById(33)->attributeCount == 0, "Tackle unexpected upstream move attributes");\nstatic_assert(levelMovesFor(*findSpeciesByDex(1))[0].moveId == 33, "Bulbasaur canonical level-up moves mismatch");\ninline constexpr char kStartingBiomeId[] = "${field(content.extensions?.upstreamStartingBiome?.id ?? 'plains')}";\n}\n`;
const expandedHeader = header
  .replace('inline constexpr Entity kItems[] = {', `struct ModifierPoolEntry { const char* pool; const char* tier; uint16_t slot; const char* itemId; double staticWeight; double maxWeight; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr ModifierPoolEntry kModifierPoolEntries[] = {\n${modifierPoolRows}\n};\ninline constexpr Entity kItems[] = {`)
  .replace('inline constexpr std::size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);', 'inline constexpr std::size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]); inline constexpr std::size_t kModifierPoolEntryCount = sizeof(kModifierPoolEntries) / sizeof(kModifierPoolEntries[0]);')
  .replace(
    'struct Entity { const char* id; const char* name; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };',
    'struct Entity { const char* id; const char* name; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; }; struct TrainerType { uint16_t id; uint16_t derivedTypeId; uint32_t moneyMultiplierMilli; uint8_t flags; uint32_t partyTemplateOffset; uint16_t partyTemplateCount; uint32_t callbackTemplateOffset; uint16_t callbackTemplateCount; uint32_t signatureOffset; uint8_t signatureCount; uint32_t speciesPoolOffset; uint16_t speciesPoolCount; const char* key; const char* name; const char* specialtyType; bool specialtyTypeResolved; const char* partyTemplateStatus; const char* configStatus; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; const char* configSourcePath; const char* configSourceSymbol; const char* configSourceHash; }; struct TrainerPartyTemplateRef { uint16_t trainerId; const char* templateKey; uint16_t maxWave; }; struct TrainerSignatureSpecies { const char* speciesId; }; struct TrainerSignatureChoice { uint16_t trainerId; uint8_t slot; bool isGroup; uint32_t speciesOffset; uint8_t speciesCount; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; }; struct TrainerPoolSpecies { const char* speciesId; }; struct TrainerPoolChoice { uint16_t memberIndex; uint32_t speciesOffset; uint8_t speciesCount; bool isGroup; }; struct TrainerSpeciesPool { uint16_t trainerId; uint8_t tierId; uint32_t candidateOffset; uint16_t candidateCount; const char* tier; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };'
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
  .replace('struct BiomeTrainerPoolEntry { const char* biomeId; const char* tier; const char* trainerId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };', 'struct BiomeTrainerPoolEntry { const char* biomeId; const char* tier; uint16_t memberIndex; const char* trainerId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; }; struct TrainerPartySegment { uint8_t size; uint8_t strengthId; bool sameSpecies; bool balanced; uint8_t evolutionThresholdKindId; const char* strength; const char* evolutionThresholdKind; }; struct TrainerPartyTemplate { const char* key; uint8_t totalSize; bool isCompound; uint32_t segmentOffset; uint16_t segmentCount; uint8_t parentEvolutionThresholdKindId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };')
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
    'enum MoveCategory : uint8_t { MovePhysical = 0, MoveSpecial = 1, MoveStatus = 2 }; enum MoveUpstreamFlags : uint16_t { MoveIsUnimplemented = 1, MoveHasSacrificialAttrOnHit = 2, MoveHasMultiHit = 4, MoveHasMultiHitPowerIncrement = 8, MoveHasDelayedAttack = 16, MoveHasRecharge = 32, MoveIsCharging = 64, MoveChecksAccuracyPerHit = 128, MoveUsesDefense = 256, MoveSelectsOffensiveCategory = 512, MoveHasSacrificialAttr = 1024, MoveIsStabBlacklisted = 2048, MoveHasStrongSelfStatBoost = 4096, MoveRequiresPostSelectionFilter = 8192, MoveHasVariableMovegenType = 16384 };'
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
const trainerMoveHeader = atlasHeader
  .replace('struct MoveAttribute {', 'struct MoveSupercedence { uint16_t moveId; uint16_t replacementMoveId; }; struct ForcedSignatureMove { uint16_t speciesDex; uint16_t moveId; bool isArray; bool rival; }; struct MoveAttribute {')
  .replace('inline constexpr SpeciesLevelMove kSpeciesLevelMoves[] = {',
    `inline constexpr MoveSupercedence kMoveSupercedence[] = {\n${supercedenceRows}\n};\ninline constexpr char kMoveSupercedenceSourcePath[] = "${field(supercedence.provenance.sourcePath)}";\ninline constexpr char kMoveSupercedenceSourceHash[] = "${field(supercedence.provenance.sourceHash)}";\ninline constexpr SpeciesLevelMove kSpeciesLevelMoves[] = {`)
  .replace('inline constexpr std::size_t kSpeciesLevelMoveCount =',
    'inline constexpr std::size_t kMoveSupercedenceCount = sizeof(kMoveSupercedence) / sizeof(kMoveSupercedence[0]);\ninline constexpr std::size_t kSpeciesLevelMoveCount =')
  .replace('inline constexpr SpeciesLevelMove kSpeciesLevelMoves[] = {',
    `inline constexpr uint16_t kForbiddenSinglesMoveIds[] = {\n${moveBlocklistRows('singles')}\n};\ninline constexpr uint16_t kLevelBasedDeniedMoveIds[] = {\n${moveBlocklistRows('levelBased')}\n};\ninline constexpr uint16_t kForbiddenTmMoveIds[] = {\n${moveBlocklistRows('tm')}\n};\ninline constexpr char kMoveBlocklistsSourcePath[] = "${field(moveBlocklists.provenance.sourcePath)}";\ninline constexpr char kMoveBlocklistsSourceHash[] = "${field(moveBlocklists.provenance.sourceHash)}";\ninline constexpr ForcedSignatureMove kForcedSignatureMoves[] = {\n${signatureRows}\n};\ninline constexpr uint8_t kForcedSignatureMoveChance = ${forcedSignatures.chancePercent};\ninline constexpr char kForcedSignatureSourcePath[] = "${field(forcedSignatures.provenance.sourcePath)}";\ninline constexpr char kForcedSignatureSourceHash[] = "${field(forcedSignatures.provenance.sourceHash)}";\ninline constexpr SpeciesLevelMove kSpeciesLevelMoves[] = {`);
await fs.mkdir(path.dirname(outputPath), { recursive: true });
const statStageHeader = trainerMoveHeader.replace(
  'struct MoveAttribute {',
  `struct MoveStatStageEffect { uint16_t moveId; uint8_t statMask; int8_t stages; bool selfTarget; };\ninline constexpr MoveStatStageEffect kMoveStatStageEffects[] = {\n${moveStatStageEffects.join(',\n')}\n};\ninline constexpr std::size_t kMoveStatStageEffectCount = sizeof(kMoveStatStageEffects) / sizeof(kMoveStatStageEffects[0]);\nstruct MoveAttribute {`
);
const abilityStatStageHeader = statStageHeader.replace(
  'struct MoveAttribute {',
  `struct AbilityStatStageProfile { uint16_t abilityId; int8_t multiplier; uint8_t protectedMask; bool reflectDrops; bool copiesRaises; bool ignorable; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr AbilityStatStageProfile kAbilityStatStageProfiles[] = {\n${abilityStatStageRows.join(',\n')}\n};\ninline constexpr const AbilityStatStageProfile* findAbilityStatStageProfile(uint16_t id) { for (const auto& profile : kAbilityStatStageProfiles) if (profile.abilityId == id) return &profile; return nullptr; }\nstruct MoveAttribute {`
);
const reactionHeader = abilityStatStageHeader.replace('struct MoveAttribute {',
  `struct AbilityStatStageReaction { uint16_t abilityId; uint8_t stat; uint8_t stagesPerRequestedStat; const char* sourceSymbol; };\ninline constexpr AbilityStatStageReaction kAbilityStatStageReactions[] = {\n${abilityStatReactionRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const itemStageHeader = reactionHeader.replace('struct MoveAttribute {',
  `struct NegativeStageResetItemProfile { const char* itemId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr NegativeStageResetItemProfile kNegativeStageResetItemProfiles[] = {\n${negativeStageResetItemRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const damageAbilityHeader = itemStageHeader.replace('struct MoveAttribute {',
  `struct LowHpTypePowerAbility { uint16_t abilityId; const char* type; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr LowHpTypePowerAbility kLowHpTypePowerAbilities[] = {\n${lowHpTypePowerRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const typePowerHeader = damageAbilityHeader.replace('struct MoveAttribute {',
  `struct TypePowerAbility { uint16_t abilityId; const char* type; double multiplier; bool requiresCondition; const char* conditionWeatherSymbol; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr TypePowerAbility kTypePowerAbilities[] = {\n${typePowerAbilityRows.join(',\n')}\n};\nstruct MoveAttribute {`);
// Normalize the inspected pinned WeatherPool declarations from preserved raw
// records. Unknown syntax fails generation rather than producing neutral weather.
const biomeWeatherRows = [];
for (const biome of [...collections.biomes].sort((a, b) => a.id.localeCompare(b.id))) {
  const raw = biome.extensions?.upstreamRawRecord?.value;
  const match = typeof raw === 'string' && raw.match(/const\s+weatherPool\s*:\s*WeatherPool\s*=\s*\{([\s\S]*?)\};/);
  if (!match) throw new Error(`Missing pinned weatherPool: ${biome.id}`);
  const body = match[1].replace(/\/\/[^\n]*/g, '').trim();
  const entries = [...body.matchAll(/\[WeatherType\.([A-Z_]+)\]\s*:\s*(\d+)\s*,?/g)];
  const remainder = body.replace(/\[WeatherType\.[A-Z_]+\]\s*:\s*\d+\s*,?/g, '').trim();
  if (remainder || !entries.length) throw new Error(`Unsupported pinned weatherPool syntax: ${biome.id}`);
  const seen = new Set();
  for (const entry of entries.sort((a, b) => a[1].localeCompare(b[1]))) {
    if (seen.has(entry[1])) throw new Error(`Duplicate weatherPool key: ${biome.id}/${entry[1]}`);
    seen.add(entry[1]);
    const weight = Number(entry[2]);
    if (!Number.isSafeInteger(weight) || weight > 65535) throw new Error(`Unsupported weather weight: ${biome.id}`);
    biomeWeatherRows.push(`    {"${field(biome.id)}", "${field(entry[1])}", ${weight}, "${field(biome.provenance.sourcePath)}", "weatherPool.${field(entry[1])}", "${field(biome.provenance.sourceHash)}"}`);
  }
}
const weatherHeader = typePowerHeader.replace('struct MoveAttribute {',
  `struct BiomeWeatherPoolEntry { const char* biomeId; const char* weatherSymbol; uint16_t weight; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr BiomeWeatherPoolEntry kBiomeWeatherPools[] = {\n${biomeWeatherRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const weatherAbilityRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const suppress = [...raw.matchAll(/\.attr\s*\(\s*SuppressWeatherEffectAbAttr\s*(?:,\s*(true|false)\s*)?\)/g)];
  const override = [...raw.matchAll(/\.attr\s*\(\s*PreAttackWeatherOverrideAbAttr\s*,\s*WeatherType\.([A-Z_]+)\s*\)/g)];
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*(?:SuppressWeatherEffectAbAttr|PreAttackWeatherOverrideAbAttr)\b/g)];
  if (declarations.length !== suppress.length + override.length || override.length > 1)
    throw new Error(`Unsupported weather ability component: ${ability.id}`);
  if (!declarations.length) return [];
  return [`    {${ability.abilityId}, ${suppress.length > 0}, ${suppress.some(match => match[1] === 'true')}, "${field(override[0]?.[1] ?? '')}", "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`];
});
const resolvedWeatherHeader = weatherHeader.replace('struct MoveAttribute {',
  `struct WeatherAbilityProfile { uint16_t abilityId; bool suppressesWeather; bool affectsImmutable; const char* overrideWeatherSymbol; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr WeatherAbilityProfile kWeatherAbilityProfiles[] = {\n${weatherAbilityRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const weatherNames = ['NONE', 'SUNNY', 'RAIN', 'SANDSTORM', 'HAIL', 'SNOW', 'FOG', 'HEAVY_RAIN', 'HARSH_SUN', 'STRONG_WINDS'];
const weatherDamageAbilityRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const blocksIndirect = /\.attr\s*\(\s*BlockNonDirectDamageAbAttr\s*\)/.test(raw);
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*BlockWeatherDamageAttr\b/g)];
  const matches = [...raw.matchAll(/\.attr\s*\(\s*BlockWeatherDamageAttr\s*((?:,\s*WeatherType\.[A-Z_]+\s*)*)\)/g)];
  if (declarations.length !== matches.length) throw new Error(`Unsupported weather blocker: ${ability.id}`);
  if (!blocksIndirect && !matches.length) return [];
  let mask = 0;
  for (const match of matches) {
    const names = [...match[1].matchAll(/WeatherType\.([A-Z_]+)/g)].map(entry => entry[1]);
    if (!names.length) mask = 1023;
    for (const name of names) {
      const index = weatherNames.indexOf(name);
      if (index < 0) throw new Error(`Unknown weather blocker enum: ${name}`);
      mask |= 1 << index;
    }
  }
  return [`    {${ability.abilityId}, ${blocksIndirect}, ${mask}, "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`];
});
const weatherDamageHeader = resolvedWeatherHeader.replace('struct MoveAttribute {',
  `struct WeatherDamageAbilityProfile { uint16_t abilityId; bool blocksIndirectDamage; uint16_t weatherMask; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr WeatherDamageAbilityProfile kWeatherDamageAbilityProfiles[] = {\n${weatherDamageAbilityRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const moveWeatherOverrideRows = collections.moves.flatMap(move => {
  const raw = move.extensions?.upstreamEffectMetadata?.value ?? '';
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*OverrideWeatherMultiplierAttr\b/g)];
  const matches = [...raw.matchAll(/\.attr\s*\(\s*OverrideWeatherMultiplierAttr\s*,\s*WeatherType\.([A-Z_]+)\s*\)/g)];
  if (declarations.length !== matches.length) throw new Error(`Unsupported move weather override: ${move.id}`);
  return matches.map(match => {
    const weatherId = weatherNames.indexOf(match[1]);
    if (weatherId < 0) throw new Error(`Unknown move weather override: ${match[1]}`);
    return `    {${move.moveId}, ${weatherId}, "${field(move.provenance?.sourcePath ?? move.source?.sourcePath ?? '')}", "${field(move.provenance?.sourceSymbol ?? move.source?.sourceSymbol ?? '')}"}`;
  });
});
const moveWeatherHeader = weatherDamageHeader.replace('struct MoveAttribute {',
  `struct MoveWeatherOverride { uint16_t moveId; uint8_t weatherId; const char* sourcePath; const char* sourceSymbol; };\ninline constexpr MoveWeatherOverride kMoveWeatherOverrides[] = {\n${moveWeatherOverrideRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const criticalAbilityRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const bonus = [...raw.matchAll(/\.attr\s*\(\s*BonusCritAbAttr\s*\)/g)].length;
  const block = [...raw.matchAll(/\.attr\s*\(\s*BlockCritAbAttr\s*\)/g)].length;
  const multipliers = [...raw.matchAll(/\.attr\s*\(\s*MultCritAbAttr\s*,\s*(\d+(?:\.\d+)?)\s*\)/g)];
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*(?:BonusCritAbAttr|BlockCritAbAttr|MultCritAbAttr)\b/g)].length;
  if (declarations !== bonus + block + multipliers.length || bonus > 3) throw new Error(`Unsupported critical ability: ${ability.id}`);
  if (!declarations) return [];
  const multiplier = multipliers.reduce((value, match) => value * Number(match[1]), 1);
  if (!Number.isFinite(multiplier) || multiplier <= 0 || multiplier > 16) throw new Error(`Invalid critical multiplier: ${ability.id}`);
  return [`    {${ability.abilityId}, ${bonus}, ${block > 0}, ${multiplier}, ${/\.ignorable\s*\(/.test(raw)}, "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`];
});
const criticalHeader = moveWeatherHeader.replace('struct MoveAttribute {',
  `struct CriticalAbilityProfile { uint16_t abilityId; uint8_t bonusStages; bool blocksCritical; double criticalMultiplier; bool ignorable; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr CriticalAbilityProfile kCriticalAbilityProfiles[] = {\n${criticalAbilityRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const alwaysHitRows = collections.abilities.filter(ability =>
  /\.attr\s*\(\s*AlwaysHitAbAttr\s*\)/.test(ability.extensions?.upstreamAttributes?.value ?? '')
).map(ability => `    {${ability.abilityId}, "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`);
const hitHeader = criticalHeader.replace('struct MoveAttribute {',
  `struct AlwaysHitAbilityProfile { uint16_t abilityId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr AlwaysHitAbilityProfile kAlwaysHitAbilityProfiles[] = {\n${alwaysHitRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const accuracyAbilityRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  return [...raw.matchAll(/\.attr\s*\(\s*StatMultiplierAbAttr\s*,\s*Stat\.(ACC|EVA)\s*,\s*(\d+(?:\.\d+)?)\s*(?:,\s*\(_user,\s*_target,\s*move\)\s*=>\s*move\.category\s*===\s*MoveCategory\.(PHYSICAL|SPECIAL)\s*)?\)/g)].map(match => {
    const multiplier = Number(match[2]);
    if (!Number.isFinite(multiplier) || multiplier <= 0 || multiplier > 16) throw new Error(`Invalid accuracy multiplier: ${ability.id}`);
    const requiresCondition = /\.condition\s*\(/.test(raw);
    const condition = raw.match(/\.condition\s*\(\s*getWeatherCondition\s*\(([^)]*)\)\s*\)/);
    let weatherMask = 0;
    if (condition && [...raw.matchAll(/\.condition\s*\(/g)].length === 1) {
      const remainder = condition[1].replace(/WeatherType\.[A-Z_]+|,|\s/g, '');
      if (!remainder) for (const entry of condition[1].matchAll(/WeatherType\.([A-Z_]+)/g)) {
        const index = weatherNames.indexOf(entry[1]);
        if (index < 0) throw new Error(`Unknown accuracy weather condition: ${entry[1]}`);
        weatherMask |= 1 << index;
      }
    }
    return `    {${ability.abilityId}, ${match[1] === 'ACC'}, ${multiplier}, ${match[3] === 'PHYSICAL' ? 0 : match[3] === 'SPECIAL' ? 1 : -1}, ${requiresCondition}, ${weatherMask}, "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`;
  });
});
const accuracyHeader = hitHeader.replace('struct MoveAttribute {',
  `struct AccuracyAbilityProfile { uint16_t abilityId; bool accuracy; double multiplier; int8_t requiredCategory; bool requiresCondition; uint16_t weatherMask; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr AccuracyAbilityProfile kAccuracyAbilityProfiles[] = {\n${accuracyAbilityRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const damageStatRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  return [...raw.matchAll(/\.attr\s*\(\s*StatMultiplierAbAttr\s*,\s*Stat\.(ATK|DEF|SPATK|SPDEF)\s*,\s*(\d+(?:\.\d+)?)\s*\)/g)].map(match => {
    const stat = { ATK: 1, DEF: 2, SPATK: 3, SPDEF: 4 }[match[1]];
    const multiplier = Number(match[2]);
    if (!Number.isFinite(multiplier) || multiplier <= 0 || multiplier > 16) throw new Error(`Invalid damage stat multiplier: ${ability.id}`);
    return `    {${ability.abilityId}, ${stat}, ${multiplier}, ${/\.condition\s*\(/.test(raw)}, "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`;
  });
});
const statHeader = accuracyHeader.replace('struct MoveAttribute {',
  `struct DamageStatAbilityProfile { uint16_t abilityId; uint8_t stat; double multiplier; bool requiresCondition; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr DamageStatAbilityProfile kDamageStatAbilityProfiles[] = {\n${damageStatRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const speedAbilityRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*StatMultiplierAbAttr\s*,\s*Stat\.SPD\b/g)];
  const matches = [...raw.matchAll(/\.attr\s*\(\s*StatMultiplierAbAttr\s*,\s*Stat\.SPD\s*,\s*(\d+(?:\.\d+)?)\s*\)/g)];
  if (declarations.length !== matches.length) throw new Error(`Unsupported speed constructor: ${ability.id}`);
  const requiresCondition = /\.condition\s*\(/.test(raw);
  const condition = raw.match(/\.condition\s*\(\s*getWeatherCondition\s*\(([^)]*)\)\s*\)/);
  let weatherMask = 0;
  if (condition && [...raw.matchAll(/\.condition\s*\(/g)].length === 1 &&
      !condition[1].replace(/WeatherType\.[A-Z_]+|,|\s/g, '')) {
    for (const entry of condition[1].matchAll(/WeatherType\.([A-Z_]+)/g)) {
      const index = weatherNames.indexOf(entry[1]);
      if (index < 0) throw new Error(`Unknown speed weather: ${entry[1]}`);
      weatherMask |= 1 << index;
    }
  }
  return matches.map(match => {
    const multiplier = Number(match[1]);
    if (!Number.isFinite(multiplier) || multiplier <= 0 || multiplier > 16) throw new Error(`Invalid speed multiplier: ${ability.id}`);
    return `    {${ability.abilityId}, ${multiplier}, ${requiresCondition}, ${weatherMask}, "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`;
  });
});
const speedHeader = statHeader.replace('struct MoveAttribute {',
  `struct SpeedAbilityProfile { uint16_t abilityId; double multiplier; bool requiresCondition; uint16_t weatherMask; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr SpeedAbilityProfile kSpeedAbilityProfiles[] = {\n${speedAbilityRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const trickRoomRows = collections.moves.flatMap(move => {
  const raw = move.extensions?.upstreamEffectMetadata?.value ?? '';
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*AddArenaTagAttr\s*,\s*ArenaTagType\.TRICK_ROOM\b/g)];
  const matches = [...raw.matchAll(/\.attr\s*\(\s*AddArenaTagAttr\s*,\s*ArenaTagType\.TRICK_ROOM\s*,\s*(\d+)\s*\)/g)];
  if (declarations.length !== matches.length) throw new Error(`Unsupported Trick Room constructor: ${move.id}`);
  return matches.map(match => {
    const duration = Number(match[1]);
    if (!Number.isInteger(duration) || duration < 1 || duration > 65535) throw new Error(`Invalid Trick Room duration: ${move.id}`);
    return `    {${move.moveId}, ${duration}}`;
  });
});
const roomHeader = speedHeader.replace('struct MoveAttribute {',
  `struct TrickRoomMoveProfile { uint16_t moveId; uint16_t duration; };\ninline constexpr TrickRoomMoveProfile kTrickRoomMoveProfiles[] = {\n${trickRoomRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const ppAbilityRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*IncreasePpUsedAbAttr\b/g)];
  const matches = [...raw.matchAll(/\.attr\s*\(\s*IncreasePpUsedAbAttr\s*(?:,\s*(\d+)\s*)?\)/g)];
  if (declarations.length !== matches.length) throw new Error(`Unsupported PP constructor: ${ability.id}`);
  return matches.map(match => {
    const increase = Number(match[1] ?? 1);
    if (!Number.isInteger(increase) || increase > 254) throw new Error(`Invalid PP increase: ${ability.id}`);
    return `    {${ability.abilityId}, ${increase}, "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`;
  });
});
const ppHeader = roomHeader.replace('struct MoveAttribute {',
  `struct PpAbilityProfile { uint16_t abilityId; uint8_t increase; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr PpAbilityProfile kPpAbilityProfiles[] = {\n${ppAbilityRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const flaggedMoveRows = collections.moves.flatMap(move => {
  const raw = move.extensions?.upstreamEffectMetadata?.value ?? '';
  const mask = (/\.soundBased\s*\(\s*\)/.test(raw) ? 1 : 0) |
    (/\.powderMove\s*\(\s*\)/.test(raw) ? 2 : 0);
  return mask ? [`    {${move.moveId}, ${mask}}`] : [];
});
const immunityRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  if (!/\bMoveImmunityAbAttr\b/.test(raw)) return [];
  const sound = /pokemon\s*!==\s*attacker\s*&&\s*move\.hasFlag\(MoveFlags\.SOUND_BASED\)/.test(raw);
  const powder = /pokemon\s*!==\s*attacker\s*&&\s*move\.hasFlag\(MoveFlags\.POWDER_MOVE\)/.test(raw);
  return [`    {${ability.abilityId}, ${(sound ? 1 : 0) | (powder ? 2 : 0)}, ${!sound && !powder}, "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`];
});
const immunityHeader = ppHeader.replace('struct MoveAttribute {',
  `struct MoveImmunityFlags { uint16_t moveId; uint8_t mask; };\ninline constexpr MoveImmunityFlags kMoveImmunityFlags[] = {\n${flaggedMoveRows.join(',\n')}\n};\nstruct MoveImmunityAbilityProfile { uint16_t abilityId; uint8_t mask; bool requiresDispatcher; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr MoveImmunityAbilityProfile kMoveImmunityAbilityProfiles[] = {\n${immunityRows.join(',\n')}\n};\nstruct MoveAttribute {`);
// Preserve constant HealAttr constructor semantics; variable/callback healing remains raw.
const healRows = collections.moves.flatMap(move => {
  const raw = move.extensions?.upstreamEffectMetadata?.value ?? '';
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*HealAttr\b/g)];
  if (!declarations.length) return [];
  const parsed = [...raw.matchAll(/\.attr\s*\(\s*HealAttr\s*,\s*(\d+(?:\.\d+)?)(?:\s*,\s*(true|false))?(?:\s*,\s*(true|false))?(?:\s*,\s*(true|false))?\s*\)/g)];
  if (parsed.length !== declarations.length) return []; // Explicitly unsupported by native resolver.
  return parsed.map(m => {
    const ratio = Number(m[1]);
    if (!(ratio > 0 && ratio <= 1)) throw new Error(`Invalid constant healing ratio: ${move.id}`);
    return `    {${move.moveId}, ${ratio}, ${m[2] === 'true'}, ${m[3] !== 'false'}, ${m[4] !== 'false'}}`;
  });
});
const healingHeader = immunityHeader.replace('struct MoveAttribute {',
  `struct MoveHealProfile { uint16_t moveId; double ratio; bool showAnimation; bool selfTarget; bool failOnFullHp; };\ninline constexpr MoveHealProfile kMoveHealProfiles[] = {\n${healRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const drainRows = collections.moves.flatMap(move => {
  const raw = move.extensions?.upstreamEffectMetadata?.value ?? '';
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*HitHealAttr\b/g)];
  const parsed = [...raw.matchAll(/\.attr\s*\(\s*HitHealAttr(?:\s*,\s*(\d+(?:\.\d+)?))?\s*\)/g)];
  if (!declarations.length || declarations.length !== parsed.length) return [];
  return parsed.map(m => {
    const ratio = m[1] === undefined ? 0.5 : Number(m[1]);
    if (!(ratio > 0 && ratio <= 1)) throw new Error(`Invalid drain ratio: ${move.id}`);
    return `    {${move.moveId}, ${ratio}}`;
  });
});
const reverseDrainRows = collections.abilities.filter(ability =>
  /\bReverseDrainAbAttr\b/.test(ability.extensions?.upstreamAttributes?.value ?? '')
).map(ability => `    {${ability.abilityId}}`);
const drainHeader = healingHeader.replace('struct MoveAttribute {',
  `struct MoveDrainProfile { uint16_t moveId; double ratio; };\ninline constexpr MoveDrainProfile kMoveDrainProfiles[] = {\n${drainRows.join(',\n')}\n};\nstruct ReverseDrainProfile { uint16_t abilityId; };\ninline constexpr ReverseDrainProfile kReverseDrainProfiles[] = {\n${reverseDrainRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const recoilRows = collections.moves.flatMap(move => {
  const raw = move.extensions?.upstreamEffectMetadata?.value ?? '';
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*RecoilAttr\b/g)];
  const parsed = [...raw.matchAll(/\.attr\s*\(\s*RecoilAttr(?:\s*,\s*(true|false))?(?:\s*,\s*(\d+(?:\.\d+)?))?(?:\s*,\s*(true|false))?\s*\)/g)];
  if (!declarations.length || declarations.length !== parsed.length) return [];
  return parsed.map(m => {
    const ratio = m[2] === undefined ? 0.25 : Number(m[2]);
    if (!(ratio > 0 && ratio <= 1)) throw new Error(`Invalid recoil ratio: ${move.id}`);
    return `    {${move.moveId}, ${m[1] === 'true'}, ${ratio}, ${m[3] === 'true'}}`;
  });
});
const recoilAbilityRows = collections.abilities.flatMap(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const recoil = /\.attr\s*\(\s*BlockRecoilDamageAttr\s*\)/.test(raw);
  const indirect = /\.attr\s*\(\s*BlockNonDirectDamageAbAttr\s*\)/.test(raw);
  return recoil || indirect ? [`    {${ability.abilityId}, ${recoil}, ${indirect}}`] : [];
});
const recoilHeader = drainHeader.replace('struct MoveAttribute {',
  `struct MoveRecoilProfile { uint16_t moveId; bool useMaxHp; double ratio; bool unblockable; };\ninline constexpr MoveRecoilProfile kMoveRecoilProfiles[] = {\n${recoilRows.join(',\n')}\n};\nstruct RecoilAbilityProfile { uint16_t abilityId; bool blocksRecoil; bool blocksIndirectDamage; };\ninline constexpr RecoilAbilityProfile kRecoilAbilityProfiles[] = {\n${recoilAbilityRows.join(',\n')}\n};\nstruct MoveAttribute {`);
// WeatherChangeAttr parameters are preserved from the imported pinned declarations.
const weatherChangeRows = collections.moves.flatMap(move => {
  const raw = move.extensions?.upstreamEffectMetadata?.value ?? '';
  const declarations = [...raw.matchAll(/\.attr\s*\(\s*WeatherChangeAttr\b/g)];
  const parsed = [...raw.matchAll(/\.attr\s*\(\s*WeatherChangeAttr\s*,\s*WeatherType\.([A-Z_]+)\s*\)/g)];
  if (!declarations.length || declarations.length !== parsed.length) return [];
  return parsed.map(match => {
    const id = weatherNames.indexOf(match[1]);
    if (id < 0) throw new Error(`Unknown WeatherChangeAttr enum: ${move.id}/${match[1]}`);
    return `    {${move.moveId}, ${id}, "${field(move.source?.sourcePath ?? '')}", "${field(move.source?.sourceSymbol ?? '')}", "${field(move.source?.sourceHash ?? '')}"}`;
  });
});
const weatherChangeHeader = recoilHeader.replace('struct MoveAttribute {',
  `struct MoveWeatherChangeProfile { uint16_t moveId; uint8_t weatherType; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr MoveWeatherChangeProfile kMoveWeatherChangeProfiles[] = {\n${weatherChangeRows.join(',\n')}\n};\nstruct MoveAttribute {`);
// Fail closed for weather callbacks that do not yet have a native dispatcher.
// Keep an entry for every canonical ability, so unknown IDs cannot mean "no hooks".
const weatherLifecycleRows = collections.abilities.map(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const attrs = [...raw.matchAll(/\.attr\s*\(\s*(?:new\s+)?([A-Za-z_$][\w$]*)/g)].map(m => m[1]);
  const handled = new Set(['SuppressWeatherEffectAbAttr', 'PreAttackWeatherOverrideAbAttr', 'BlockWeatherDamageAttr']);
  const unknownHooks = attrs.some(name => (/Weather/.test(name) || name === 'IceFaceFormChangeAbAttr') && !handled.has(name));
  // Weather conditions on other effects also need a dispatcher. Existing speed
  // profiles cover their own conditions, but other callbacks cannot be ignored.
  const weatherCondition = /WeatherType\./.test(raw) && !attrs.some(name => handled.has(name));
  return `    {${ability.abilityId}, ${unknownHooks || weatherCondition}, "${field(ability.source?.sourcePath ?? '')}", "${field(ability.source?.sourceSymbol ?? '')}", "${field(ability.source?.sourceHash ?? '')}"}`;
});
const weatherLifecycleHeader = weatherChangeHeader.replace('struct MoveAttribute {',
  `struct WeatherLifecycleAbilityProfile { uint16_t abilityId; bool requiresDispatcher; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr WeatherLifecycleAbilityProfile kWeatherLifecycleAbilityProfiles[] = {\n${weatherLifecycleRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const catchRows = collections.species.map(species => {
  const raw = species.extensions?.upstreamRawRecord?.value ?? '';
  const parsed = /\bcatchRate\s*:\s*(\d+)\b/.exec(raw);
  if (!parsed || Number(parsed[1]) > 255) throw new Error(`Missing/invalid pinned catch rate: ${species.id}`);
  return `    {${species.speciesId}, ${Number(parsed[1])}, "${field(species.source?.sourcePath ?? '')}", "${field(species.source?.sourceSymbol ?? '')}", "${field(species.source?.sourceHash ?? '')}"}`;
});
const captureHeader = weatherLifecycleHeader.replace('struct MoveAttribute {',
  `struct SpeciesCatchProfile { uint16_t speciesDex; uint8_t catchRate; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr SpeciesCatchProfile kSpeciesCatchProfiles[] = {\n${catchRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const simpleEvolutionRows = evolutionEdges.filter(({ edge }) =>
  edge.level > 1 && !edge.item && !edge.condition && !edge.preFormKey && !edge.evoFormKey
).map(({ species, edge, order }) => `    {"${field(species.id)}", ${order}}`);
const evolutionCapabilityHeader = captureHeader.replace('struct MoveAttribute {',
  `struct SimpleLevelEvolutionProfile { const char* speciesId; uint16_t sourceOrder; };\ninline constexpr SimpleLevelEvolutionProfile kSimpleLevelEvolutionProfiles[] = {\n${simpleEvolutionRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const fixedMovesets = content.extensions?.fixedEnemyMovesets;
if (!fixedMovesets?.entries?.length || !fixedMovesets.provenance?.sourceHash)
  throw new Error('Missing pinned fixed enemy movesets');
const fixedRows = fixedMovesets.entries.map(profile => {
  const species = collections.species.find(entry => entry.id === profile.speciesId);
  if (!species || profile.moves.length !== 4 || profile.moves.some(move => !catalogMoveIds.has(move.moveId)))
    throw new Error('Invalid fixed enemy moveset reference');
  return `    {${species.speciesId}, ${profile.formIndex}, {${profile.moves.map(move => move.moveId).join(', ')}}, {${profile.moves.map(move => move.ppUsed).join(', ')}}, {${profile.moves.map(move => move.ppUp).join(', ')}}}`;
});
const fixedMovesetHeader = evolutionCapabilityHeader.replace('struct MoveAttribute {',
  `struct FixedEnemyMoveset { uint16_t speciesDex; uint8_t formIndex; uint16_t moveIds[4]; uint8_t ppUsed[4]; int8_t ppUp[4]; };\ninline constexpr FixedEnemyMoveset kFixedEnemyMovesets[] = {\n${fixedRows.join(',\n')}\n};\ninline constexpr Entity kFixedEnemyMovesetSource = {"fixed-enemy-movesets", "EnemyPokemon.generateAndPopulateMoveset", "${field(fixedMovesets.provenance.sourcePath)}", "${field(fixedMovesets.provenance.sourceSymbol)}", "${field(fixedMovesets.provenance.sourceHash)}"};\nstruct MoveAttribute {`);
const heldClassRows = collections.items.map(item => {
  const raw = item.extensions?.upstreamRawRecord?.value ?? '';
  const supported = /new\s+TurnHeldItemTransferModifierType\s*\(/.test(raw);
  return `    {"${field(item.id)}", ${supported}, "${field(item.source?.sourcePath)}", "${field(item.source?.sourceSymbol)}", "${field(item.source?.sourceHash)}"}`;
});
const heldClassHeader = fixedMovesetHeader.replace('struct MoveAttribute {',
  `struct HeldModifierClassProfile { const char* itemId; bool isTurnHeldItemTransfer; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr HeldModifierClassProfile kHeldModifierClassProfiles[] = {\n${heldClassRows.join(',\n')}\n};\nstruct MoveAttribute {`);
const theftAbilityRows = collections.abilities.map(ability => {
  const raw = ability.extensions?.upstreamAttributes?.value ?? '';
  const attrs = [...new Set([...raw.matchAll(/\b([A-Za-z_$][\w$]*(?:ItemTheft|ItemLost)[\w$]*AbAttr)\b/g)].map(match => match[1]))];
  const blocks = attrs.includes('BlockItemTheftAbAttr');
  const pending = attrs.some(attr => attr !== 'BlockItemTheftAbAttr' && attr !== 'PostItemLostAbAttr');
  const conditional = attrs.length > 0 && /\.(?:condition|conditionalAttr)\s*\(/.test(raw);
  return `    {${ability.abilityId}, ${blocks}, ${pending}, ${conditional}, "${field(ability.source?.sourcePath)}", "${field(ability.source?.sourceSymbol)}", "${field(ability.source?.sourceHash)}"}`;
});
const theftAbilityHeader = heldClassHeader.replace('struct MoveAttribute {',
  `struct HeldItemTheftAbilityProfile { uint16_t abilityId; bool blocksTheft; bool requiresPostLostDispatcher; bool conditionalCallbacks; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr HeldItemTheftAbilityProfile kHeldItemTheftAbilityProfiles[] = {\n${theftAbilityRows.join(',\n')}\n};\nstruct MoveAttribute {`);
await fs.writeFile(outputPath, theftAbilityHeader, 'utf8');
console.log(JSON.stringify({ output: path.relative(root, outputPath), bytes: Buffer.byteLength(theftAbilityHeader), hash: report.contentHash }));
