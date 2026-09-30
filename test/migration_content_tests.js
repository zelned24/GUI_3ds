import assert from 'assert';
import { readFile } from 'node:fs/promises';
import { PokerogueImporter } from '../tools/js/data/PokerogueImporter.js';
import { PokerogueRepository } from '../tools/js/data/PokerogueRepository.js';
import { EncounterResolver, FirstRunFlow } from '../tools/js/game/FirstRunFlow.js';
import { CanonicalContent, RuntimeContent } from '../tools/js/data/CanonicalDataContract.js';
import { DataManager } from '../tools/js/data/DataManager.js';

export function registerMigrationContentTests(test) {
  test('Content migration: pinned upstream content completes a deterministic first-run presentation flow', async () => {
    const importer = new PokerogueImporter(new PokerogueRepository());
    const imported = await importer.importPlayableCanonicalContent(undefined, { generations: [1] });
    const repeatedImport = await importer.importPlayableCanonicalContent(undefined, { generations: [1] });
    assert.strictEqual(repeatedImport.importReport.contentHash, imported.importReport.contentHash, 'same pins/import/normalization produce the same canonical hash');
    const pikachu = imported.species.find(species => species.id === 'pikachu');
    assert.strictEqual(pikachu.baseTotal, 320, 'explicit upstream baseTotal is normalized as canonical data');
    assert.strictEqual(pikachu.growthRate, 'MEDIUM_FAST', 'growth-rate identifier is normalized without a gameplay port');
    const pikachuGigantamax = imported.canonicalContent.collections.forms.find(form => form.speciesId === 'pikachu' && form.formKey === 'GIGANTAMAX');
    assert.ok(pikachuGigantamax.levelMoves.some(move => move.move === 'zippy_zap' && move.level === 20), 'pinned form-specific level moves reach the matching canonical form record');
    assert.deepStrictEqual(pikachu.rarity, { legendary: null, subLegendary: null, mythical: null }, 'absent upstream rarity fields remain distinguishable from explicit false');
    const legendary = imported.species.find(species => species.rarity.legendary === true);
    assert.ok(legendary, 'explicit upstream legendary classification is retained');
    assert.strictEqual(imported.canonicalContent.extensions.biomePoolReferenceAudit.status, 'PARTIAL_SPECIES_SNAPSHOT_UNVERIFIED', 'filtered generation imports declare cross-reference limits');
    const plains = imported.canonicalContent.collections.biomes.find(biome => biome.id === 'plains');
    const poolSpecies = [...new Set(Object.values(plains.encounterPools).flatMap(tier => Object.values(tier).flat()))].map(id => ({ id }));
    const poolResolver = new EncounterResolver();
    const ruleSource = imported.canonicalContent.extensions.upstreamEncounterSelection;
    assert.strictEqual(ruleSource.sourcePath, 'src/field/arena.ts');
    assert.ok(imported.sourceSnapshot.sources.some(source => source.sourcePath === 'src/enums/biome-pool-tier.ts'));
    const commonPick = poolResolver.selectPoolMember({ biome: plains, timeOfDay: 'day', tierRoll: 156, memberIndex: 0, species: poolSpecies, sourceRef: { ...ruleSource, sourceType: 'UPSTREAM' } });
    assert.deepStrictEqual([commonPick.requestedTier, commonPick.selectedTier, commonPick.speciesId, commonPick.poolSize], ['common', 'common', 'zigzagoon', 6]);
    const uncommonPick = poolResolver.selectPoolMember({ biome: plains, timeOfDay: 'day', tierRoll: 155, memberIndex: 0, species: poolSpecies, sourceRef: { ...ruleSource, sourceType: 'UPSTREAM' } });
    assert.strictEqual(uncommonPick.requestedTier, 'uncommon', 'non-boss tier boundary 155 maps to Uncommon per upstream Arena');
    assert.strictEqual(uncommonPick.speciesId, plains.encounterPools.uncommon.all[0]);
    const bossPick = poolResolver.selectPoolMember({ biome: plains, timeOfDay: 'day', boss: true, tierRoll: 20, memberIndex: 0, species: poolSpecies, sourceRef: { ...ruleSource, sourceType: 'UPSTREAM' } });
    assert.strictEqual(bossPick.requestedTier, 'boss', 'boss tier boundary 20 maps to Boss per upstream Arena');
    for (const [tierRoll, expectedTier] of [[511, 'common'], [155, 'uncommon'], [31, 'rare'], [5, 'super_rare'], [0, 'ultra_rare']]) {
      assert.strictEqual(poolResolver.selectPoolMember({ biome: plains, timeOfDay: 'day', tierRoll, memberIndex: 0, species: poolSpecies, sourceRef: { ...ruleSource, sourceType: 'UPSTREAM' } }).requestedTier, expectedTier);
    }
    for (const [tierRoll, expectedTier] of [[63, 'boss'], [19, 'boss_rare'], [5, 'boss_super_rare'], [0, 'boss_ultra_rare']]) {
      assert.strictEqual(poolResolver.selectPoolMember({ biome: plains, timeOfDay: 'day', boss: true, tierRoll, memberIndex: 0, species: poolSpecies, sourceRef: { ...ruleSource, sourceType: 'UPSTREAM' } }).requestedTier, expectedTier);
    }
    assert.throws(() => poolResolver.selectPoolMember({ biome: plains, timeOfDay: 'day', tierRoll: 156, species: poolSpecies, sourceRef: { ...ruleSource, sourceType: 'UPSTREAM' } }), /member draw must be an integer/);
    const manager = new DataManager();
    const production = manager.loadCanonicalProductionSnapshot(imported.canonicalContent, imported.importReport);
    assert.strictEqual(manager.isFallback, false);
    const flow = new FirstRunFlow({ runtimeContent: production.runtimeContent, localeEntries: production.locales, locale: 'en' });
    const starters = flow.getStarters();
    assert.ok(starters.length > 1, 'starter list comes from pinned species source declarations');
    const firstWithForm = starters.find(species => imported.canonicalContent.collections.forms.some(form => form.speciesId === species.id));
    assert.ok(firstWithForm, 'at least one real starter has canonical form records');
    const run = flow.start({ modeId: 'classic', starterId: firstWithForm.id, seed: 9127 });
    const repeat = flow.start({ modeId: 'classic', starterId: firstWithForm.id, seed: 9127 });
    assert.strictEqual(JSON.stringify(run.encounter), JSON.stringify(repeat.encounter), 'same mode/wave/biome/seed resolves the same real encounter');
    assert.strictEqual(run.mode.id, 'classic');
    assert.strictEqual(run.run.currentMapNodeId, run.node.id);
    assert.ok(run.node.connections.length, 'first node exposes valid upstream biome routes');
    assert.strictEqual(run.biome.provenance.sourceType, 'UPSTREAM');
    assert.ok(run.presentation.biome.name, 'biome display name resolves through imported locale entries');
    const expectedForm = imported.canonicalContent.collections.forms.find(form => form.speciesId === firstWithForm.id);
    assert.strictEqual(run.presentation.playerPokemon.formId, expectedForm.id);
    assert.ok(run.presentation.playerPokemon.assetReference, 'upstream asset metadata reaches resolved Pokémon');
    assert.ok(run.presentation.playerPokemon.assetStatus.includes('PENDING'), 'physical conversion is not claimed from metadata alone');
    assert.ok(run.presentation.playerPokemon.hp.maximum > 0, 'existing prototype battle data supplies presentation HP');
    assert.strictEqual(run.presentation.playerPokemon.status, null, 'initial status is projected without inventing an ailment');
    assert.strictEqual(run.scene.bindings.enemyPokemon.speciesId, run.encounter.speciesId);
    assert.strictEqual(run.scene.bindings.playerPokemon.name, run.presentation.playerPokemon.name);
    assert.strictEqual(run.scene.bindings.playerPokemon.formId, expectedForm.id);
    const secondStarter = starters.find(species => species.id !== firstWithForm.id);
    const secondRun = flow.start({ modeId: 'classic', starterId: secondStarter.id, seed: 9127 });
    assert.strictEqual(run.scene.templateId, secondRun.scene.templateId);
    assert.ok(Object.isFrozen(flow.sceneTemplate), 'template is immutable across bindings');
    assert.notStrictEqual(run.scene.bindings.playerPokemon.speciesId, secondRun.scene.bindings.playerPokemon.speciesId, 'same template binds different real species without template mutation');
    assert.notStrictEqual(run.scene.bindings.playerPokemon.name, secondRun.scene.bindings.playerPokemon.name, 'localized species presentation changes with the bound species');
    assert.strictEqual(imported.canonicalContent.sourceSnapshot.sourceType, 'UPSTREAM');
    assert.ok(imported.importReport.contentHash);
  });

  test('Content migration: production first-run pipeline rejects fixture content', () => {
    const fixture = new CanonicalContent({ sourceSnapshot: { repository: 'test', revision: 'fixture', sourceType: 'TEST_FIXTURE' }, provenance: { sourceRepository: 'test', sourceRevision: 'fixture', sourcePath: 'test/fixtures/content.json', sourceType: 'TEST_FIXTURE' }, collections: {} });
    assert.throws(() => new FirstRunFlow({ runtimeContent: new RuntimeContent({ canonicalContent: fixture }) }), /fixture\/non-UPSTREAM/);
  });

  test('Content migration: Old 3DS C++ bundle matches pinned import report and carries real catalogs', async () => {
    const [header, reportText, canonicalText] = await Promise.all([
      readFile('project/generated/include/content/PokerogueRuntimeContent.hpp', 'utf8'),
      readFile('project/data/pokerogue/import-report.json', 'utf8'),
      readFile('project/data/pokerogue/canonical-content.json', 'utf8'),
    ]);
    const report = JSON.parse(reportText);
    const canonical = JSON.parse(canonicalText);
    assert.ok(header.includes(`kContentHash[] = "${report.contentHash}"`));
    assert.ok(header.includes(`kPokerogueRevision[] = "${report.sourceRevisions.pokerogue}"`));
    for (const domain of ['kSpecies', 'kForms', 'kMoves', 'kAbilities', 'kItems', 'kLocales', 'kModes', 'kBiomes', 'kBiomeEncounterPools', 'kBiomeTrainerPools', 'kRoutes']) {
      assert.ok(header.includes(`${domain}[] = {`), `native ROM bundle contains ${domain}`);
    }
    assert.match(header, /\{1, 0, 875, 1, 3, true, true, 318, 45, 49, 49, 65, 65, 45, 65, 65, 34, 0, 0, 17, -1, -1, -1, "MEDIUM_SLOW", "bulbasaur", "Bulbasaur"/);
    assert.ok(canonical.extensions.freshProfile.defaultStarterSpecies.includes('bulbasaur'));
    assert.ok(canonical.sourceSnapshot.sources.some(source => source.sourcePath === 'src/constants.ts'), 'fresh-profile starter allowlist is pinned and hashed');
    assert.deepStrictEqual(canonical.collections.species.find(species => species.id === 'bulbasaur').eggMoves,
      ['giga_drain', 'gunk_shot', 'earth_power', 'sappy_seed'], 'real starter egg moves are imported from the pinned balance catalog');
    assert.ok(canonical.sourceSnapshot.sources.some(source => source.sourcePath === 'src/data/balance/moves/egg-moves.ts'));
    assert.ok(header.includes('kSpeciesEggMoves[] = {') && header.includes('eggMovesFor(const Species& species)'), 'native runtime exposes canonical egg-move ranges');
    assert.ok(header.includes('uint16_t prevolutionDex'), 'native species carry registry-derived prevolutions');
    const ivysaur = canonical.collections.species.find(species => species.id === 'ivysaur');
    assert.strictEqual(ivysaur.starterEligible, false, 'evolution-line membership is not starter eligibility');
    assert.strictEqual(ivysaur.prevolutionSpeciesId, 'bulbasaur');
    assert.ok(header.includes('findSpeciesByDex(6)->malePercentTenths == 875'), 'native content preserves the upstream numeric gender ratio');
    assert.ok(header.includes('findSpeciesByDex(81)->malePercentTenths == 65534'), 'native content preserves explicit genderless null');
    assert.match(header, /\{33, 0, 40, 100, 35, 0, -1, 1, 0, 0, "tackle", "Tackle", "NORMAL", "NEAR_OTHER"/);
    assert.match(header, /\{117, 0, -1, -1, 10, 1, -1, 1, MoveIsUnimplemented, 0, "bide", "Bide"/);
    assert.match(header, /\{262, 2, -1, 100, 10, 0, -1, 3, MoveHasSacrificialAttrOnHit \| MoveHasSacrificialAttr, 0, "memento", "Memento"/);
    assert.ok(header.includes('uint16_t upstreamFlags'), 'native move metadata exposes upstream move-generation and power flags');
    assert.match(header, /MoveIsStabBlacklisted = 2048/, 'native move metadata preserves pinned forced-STAB blacklist');
    assert.match(header, /\{3, 0, 15, 85, 10, 0, -1, 1, MoveHasMultiHit, 1, "double_slap"/, 'default MultiHitAttr maps to pinned TWO_TO_FIVE');
    assert.ok(header.includes('int8_t level; uint16_t moveId;'), 'native learnset retains upstream signed sentinel levels');
    assert.ok(header.includes('levelMovesFor(const Form& form)'), 'native runtime exposes form-specific learnset ranges');
    const gigantamaxRow = header.split(/\r?\n/).find(line => line.includes('{"pikachu:gigantamax"'));
    const gigantamaxRange = gigantamaxRow?.match(/, (\d+), (\d+), "ELECTRIC"/);
    assert.ok(gigantamaxRange, 'native Pikachu Gigantamax form exposes a learnset range');
    const formOffset = Number(gigantamaxRange[1]);
    const formCount = Number(gigantamaxRange[2]);
    const formLevelMoves = canonical.collections.forms.find(form => form.id === 'pikachu:gigantamax').levelMoves;
    const canonicalMoveIds = new Map(canonical.collections.moves.map(move => [move.id, move.moveId]));
    const nativeLearnsetRows = header.slice(header.indexOf('kSpeciesLevelMoves[] = {')).split(/\r?\n/)
      .filter(line => /^\s*\{\d+, -?\d+, \d+\}/.test(line))
      .map(line => line.match(/\{(\d+), (-?\d+), (\d+)\}/).slice(1).map(Number));
    assert.deepStrictEqual(nativeLearnsetRows.slice(formOffset, formOffset + formCount).map(([, level, moveId]) => [level, moveId]),
      formLevelMoves.map(move => [move.level, canonicalMoveIds.get(move.move)]),
      'native form learnset range exactly preserves canonical form move order and levels');
    assert.match(header, /\{1, 1, 33\},\r?\n\s*\{1, 1, 45\}/, 'native learnset uses canonical MoveId references in upstream level order');
    assert.ok(header.includes('findMoveById(uint16_t id)'), 'C++ runtime exposes a compact move lookup');
    assert.ok(header.includes('levelMovesFor(const Species& species)'), 'species records expose a bounded range into shared learnset storage');
    assert.ok(header.includes('"en:pokemon:bulbasaur", "Bulbasaur"'), 'native runtime resolves names from imported locale records');
    assert.ok(header.includes('kStartingBiomeId[] = "town"'));
    assert.ok(header.includes('{"plains", "common", "dawn", 0, "sentret"'), 'native encounter pool rows preserve real Plains membership order and provenance');
  });
}
