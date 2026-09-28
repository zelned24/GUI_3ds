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
const speciesRows = collections.species.map(item => {
  const asset = collections.assetReferences.find(entry => entry.speciesId === item.nationalDexId) ?? item.extensions?.assetReference;
  const form = collections.forms.find(entry => entry.speciesId === item.id);
  const flag = value => value === true ? 1 : value === false ? 0 : -1;
  return `    {${item.nationalDexId}, ${item.generation ?? 0}, ${item.starterCost ?? -1}, ${item.starterEligible === true ? 'true' : 'false'}, ${item.baseTotal ?? 0}, ${flag(item.rarity?.legendary)}, ${flag(item.rarity?.subLegendary)}, ${flag(item.rarity?.mythical)}, "${field(item.growthRate ?? '')}", "${field(item.id)}", "${field(item.name)}", "${field(item.type1)}", "${field(item.type2)}", "${field(form?.id ?? '')}", "${field(asset?.sourcePath ?? '')}", "${field(item.source?.sourcePath ?? '')}", "${field(item.source?.sourceSymbol ?? '')}", "${field(item.source?.sourceHash ?? '')}"}`;
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
const forms = entityRows(collections.forms);
const moves = entityRows(collections.moves);
const abilities = entityRows(collections.abilities);
const items = entityRows(collections.items);

const header = `// Generated from pinned canonical PokéRogue content. Do not edit by hand.\n#pragma once\n#include <cstddef>\n#include <cstdint>\nnamespace PokerogueContent {\ninline constexpr char kContentHash[] = "${report.contentHash}";\ninline constexpr char kPokerogueRevision[] = "${report.sourceRevisions?.pokerogue ?? ''}";\ninline constexpr char kAssetsRevision[] = "${report.sourceRevisions?.assets ?? ''}";\ninline constexpr char kLocalesRevision[] = "${report.sourceRevisions?.locales ?? ''}";\nstruct Entity { const char* id; const char* name; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct Species { uint16_t dex; uint8_t generation; int8_t starterCost; bool starterEligible; uint16_t baseTotal; int8_t legendary; int8_t subLegendary; int8_t mythical; const char* growthRate; const char* id; const char* name; const char* type1; const char* type2; const char* firstFormId; const char* assetSourcePath; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct BiomeEncounterPoolEntry { const char* biomeId; const char* tier; const char* timeOfDay; uint16_t memberIndex; const char* speciesId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\nstruct BiomeTrainerPoolEntry { const char* biomeId; const char* tier; const char* trainerId; const char* sourcePath; const char* sourceSymbol; const char* sourceHash; };\ninline constexpr Species kSpecies[] = {\n${speciesRows}\n};\ninline constexpr Entity kForms[] = {\n${forms}\n};\ninline constexpr Entity kMoves[] = {\n${moves}\n};\ninline constexpr Entity kAbilities[] = {\n${abilities}\n};\ninline constexpr Entity kItems[] = {\n${items}\n};\ninline constexpr Entity kModes[] = {\n${modes}\n};\ninline constexpr Entity kBiomes[] = {\n${biomes}\n};\ninline constexpr BiomeEncounterPoolEntry kBiomeEncounterPools[] = {\n${encounterPoolRows}\n};\ninline constexpr BiomeTrainerPoolEntry kBiomeTrainerPools[] = {\n${trainerPoolRows}\n};\ninline constexpr Entity kRoutes[] = {\n${routes}\n};\ninline constexpr Entity kLocales[] = {\n${locales}\n};\ninline constexpr std::size_t kSpeciesCount = sizeof(kSpecies) / sizeof(kSpecies[0]);\ninline constexpr std::size_t kFormCount = sizeof(kForms) / sizeof(kForms[0]);\ninline constexpr std::size_t kMoveCount = sizeof(kMoves) / sizeof(kMoves[0]);\ninline constexpr std::size_t kAbilityCount = sizeof(kAbilities) / sizeof(kAbilities[0]);\ninline constexpr std::size_t kItemCount = sizeof(kItems) / sizeof(kItems[0]);\ninline constexpr std::size_t kModeCount = sizeof(kModes) / sizeof(kModes[0]);\ninline constexpr std::size_t kBiomeCount = sizeof(kBiomes) / sizeof(kBiomes[0]);\ninline constexpr std::size_t kBiomeEncounterPoolCount = sizeof(kBiomeEncounterPools) / sizeof(kBiomeEncounterPools[0]);\ninline constexpr std::size_t kBiomeTrainerPoolCount = sizeof(kBiomeTrainerPools) / sizeof(kBiomeTrainerPools[0]);\ninline constexpr std::size_t kRouteCount = sizeof(kRoutes) / sizeof(kRoutes[0]);\ninline constexpr std::size_t kLocaleCount = sizeof(kLocales) / sizeof(kLocales[0]);\ninline constexpr char kStartingBiomeId[] = "${field(content.extensions?.upstreamStartingBiome?.id ?? 'plains')}";\n}\n`;
await fs.mkdir(path.dirname(outputPath), { recursive: true });
await fs.writeFile(outputPath, header, 'utf8');
console.log(JSON.stringify({ output: path.relative(root, outputPath), bytes: Buffer.byteLength(header), hash: report.contentHash }));
