import assert from 'assert';
import { readFile, mkdir, rm } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { PokerogueImporter, parseStarterCandyPriceTable, parseRivalPartyConfiguration, parseBerryGeneration, parseBerryHeldLimits, parseBerryEffects, parseBerryPreservation, parseBerryCriticalTag, parseBerryAbilityRules, parseBerryHealingRounding } from '../tools/js/data/PokerogueImporter.js';
import { PokerogueRepository } from '../tools/js/data/PokerogueRepository.js';
import { PinnedLocalRepository } from '../scripts/PinnedLocalRepository.mjs';
import { EncounterResolver, FirstRunFlow } from '../tools/js/game/FirstRunFlow.js';
import { CanonicalContent, RuntimeContent } from '../tools/js/data/CanonicalDataContract.js';
import { DataManager } from '../tools/js/data/DataManager.js';

export function registerMigrationContentTests(test) {
  test('Runtime generator: isolated output is deterministic and cannot escape the repository', async () => {
    const root = process.cwd();
    const outputs = ['build/test-runtime-repeat-a.hpp', 'build/test-runtime-repeat-b.hpp'];
    await mkdir(path.join(root, 'build'), { recursive: true });
    const generate = output => execFileSync(process.execPath, ['scripts/generate_3ds_runtime_content.mjs'],
      { cwd: root, env: { ...process.env, POKEROGUE_RUNTIME_CONTENT_OUTPUT: output }, stdio: 'pipe' });
    try {
      for (const output of outputs) generate(output);
      const first = await readFile(path.join(root, outputs[0]));
      const second = await readFile(path.join(root, outputs[1]));
      const production = await readFile(path.join(root, 'project/generated/include/content/PokerogueRuntimeContent.hpp'));
      assert.deepEqual(first, second);
      assert.deepEqual(first, production);
      assert.throws(() => generate('../runtime-must-not-be-written.hpp'), /must stay inside the repository/);
    } finally {
      for (const output of outputs) await rm(path.join(root, output), { force: true });
    }
  });

  test('Berry healing rounding: preserve pinned helper and reject changed semantics', () => {
    const source = execFileSync('git', ['show', '8555c08c823b856cbec4eb99ca84ea52a955836d:src/utils/common.ts'],
      {cwd:path.resolve('build/upstream/pokerogue'),encoding:'utf8'});
    const rule = parseBerryHealingRounding(source);
    assert.equal(rule.kind, 'FLOOR_WITH_MINIMUM');
    assert.equal(rule.minimum, 1);
    assert.ok(rule.raw.includes('Math.floor(value)'));
    assert.throws(() => parseBerryHealingRounding(source.replace('Math.floor(value), 1', 'Math.round(value), 1')));
    assert.throws(() => parseBerryHealingRounding(source.replace('Math.floor(value), 1', 'Math.floor(value), 0')));
  });
  test('Berry ability rules: pinned ancestry distinguishes callbacks and unknown subclasses', async () => {
    const root = path.resolve('build/upstream/pokerogue');
    const source = execFileSync('git', ['show', '8555c08c823b856cbec4eb99ca84ea52a955836d:src/data/abilities/ab-attrs.ts'], { cwd: root, encoding: 'utf8' });
    const content = JSON.parse(await readFile('project/data/pokerogue/canonical-content.json', 'utf8'));
    const rules = parseBerryAbilityRules(source, content.collections.abilities);
    const row = id => rules.entries.find(entry => entry.id === id);
    assert.equal(row('ripen').resolved, true);
    assert.equal(row('ripen').effectMultiplier, 2);
    assert.equal(row('gluttony').thresholdMultiplier, 2);
    assert.equal(row('unnerve').preventsUse, true);
    assert.equal(row('cheek_pouch').healFraction, 1 / 3);
    assert.equal(row('cud_chew').cudChewConsume, true);
    assert.equal(row('cud_chew').cudChewRecord, true);
    assert.equal(row('harvest').harvest, true);
    assert.equal(row('overgrow').resolved, true);
    assert.deepEqual(row('overgrow').callbacks, []);
    const unknown = {id:'unknown', abilityId:999, extensions:{upstreamRawRecord:{value:'new AbBuilder().attr(FutureAttr).build()'}}};
    assert.equal(parseBerryAbilityRules(source, [unknown]).entries[0].resolved, false);
    unknown.extensions.upstreamRawRecord.value = 'new AbBuilder().attr(FutureRipenAttr).build()';
    const future = parseBerryAbilityRules(source + '\nexport class FutureRipenAttr extends DoubleBerryEffectAbAttr {}', [unknown]);
    assert.equal(future.entries[0].resolved, false);
    assert.deepEqual(future.entries[0].callbacks, ['DoubleBerryEffectAbAttr']);
    assert.ok(future.raw.includes('FutureRipenAttr'));
  });
  test('Berry critical tag: normalize stage count, legacy value and lapse policy', () => {
    const raw = 'if (this.tagType === BattlerTagType.DRAGON_CHEER) {} else { (this as Writable<CritBoostTag>).critStages = 2; } return lapseType !== BattlerTagLapseType.CUSTOM || super.lapse(pokemon, lapseType); (this as Writable<CritBoostTag>).critStages = source.critStages ?? 1; futureField';
    const tag = parseBerryCriticalTag(raw);
    assert.equal(tag.boostStages, 2);
    assert.equal(tag.legacyStages, 1);
    assert.equal(tag.lapseOnCustomOnly, true);
    assert.ok(tag.raw.includes('futureField'));
    assert.throws(() => parseBerryCriticalTag(raw.replace('critStages = 2', 'critStages = 3')));
    assert.throws(() => parseBerryCriticalTag(raw.replace('CUSTOM', 'TURN_END')));
  });

  test('Berry preservation: normalize seeded chance, short circuit and stack cap', () => {
    const raw = 'doPreserve.value ||= pokemon.randBattleSeedInt(10) < this.getStackCount() * 3; getMaxStackCount(): number { return 3; } futureMetadata';
    const policy = parseBerryPreservation(raw);
    assert.equal(policy.rollRange, 10);
    assert.equal(policy.chancePerStack, 3);
    assert.equal(policy.maxStacks, 3);
    assert.equal(policy.shortCircuitWhenPreserved, true);
    assert.ok(policy.raw.includes('futureMetadata'));
    assert.throws(() => parseBerryPreservation(raw.replace('||=', '=')));
    assert.throws(() => parseBerryPreservation(raw.replace('return 3', 'return 4')));
  });

  test('Berry effects: pinned semantics retain quirks and reject unknown callbacks', async () => {
    const root = path.resolve('build/upstream/pokerogue');
    const source = execFileSync('git', ['show', '8555c08c823b856cbec4eb99ca84ea52a955836d:src/data/berry.ts'], { cwd: root, encoding: 'utf8' });
    const enumEntries = ['SITRUS','LUM','ENIGMA','LIECHI','GANLON','PETAYA','APICOT','SALAC','LANSAT','STARF','LEPPA'].map((symbol,id) => [symbol,id]);
    const stats = ['HP','ATK','DEF','SPATK','SPDEF','SPD','ACC','EVA'].map((symbol,id) => [symbol,id]);
    const parsed = parseBerryEffects(source, enumEntries, stats);
    assert.equal(parsed.entries.length, 11);
    assert.ok(parsed.entries.every(row => row.resolved && row.raw.predicate && row.raw.effect));
    assert.equal(parsed.entries[0].amount, 4);
    assert.equal(parsed.entries[3].stat, 1);
    assert.equal(parsed.entries[7].stat, 5);
    assert.equal(parsed.entries[8].hpThreshold, .25);
    assert.equal(parsed.entries[9].amount, 2);
    assert.equal(parsed.entries[10].amount, 10);
    assert.equal(parsed.randomStatRange, 5);
    assert.equal(parsed.randomStatMinimum, 1);
    const changed = parseBerryEffects(source.replace('"DoubleBerryEffectAbAttr"', '"UnknownBerryCallback"'), enumEntries, stats);
    assert.equal(changed.entries[0].resolved, false);
    assert.ok(changed.raw.includes('UnknownBerryCallback'));
    assert.throws(() => parseBerryEffects(source, [...enumEntries, ['NEW_BERRY',99]], stats));
  });

  test('Berry held policy: parse source limits and reject unknown enum references', () => {
    const raw = 'matchType(modifier) { return modifier instanceof BerryModifier && modifier.berryType === this.berryType; } if ([BerryType.LUM, BerryType.SITRUS].includes(this.berryType)) { return 2; } return 3;';
    assert.deepEqual(parseBerryHeldLimits(raw, [['LUM',1],['SITRUS',0],['STARF',9]]),
      { reducedIds: [1,0], reducedMaxStacks: 2, defaultMaxStacks: 3 });
    assert.throws(() => parseBerryHeldLimits(raw.replace('BerryType.LUM', 'BerryType.UNKNOWN'), [['SITRUS',0]]));
  });
  test('Berry generator: pinned thresholds preserve enum IDs and reject broken references', () => {
    const raw = 'const rand = randSeedInt(12); if (rand < 2) { randBerryType = BerryType.SITRUS; } else if (rand < 4) { randBerryType = BerryType.LUM; } else if (rand < 6) { randBerryType = BerryType.LEPPA; } else { randBerryType = berryTypes[randSeedInt(berryTypes.length - 3) + 2]; } futureField';
    const entries = [['SITRUS',0],['LUM',1],['ENIGMA',2],['LIECHI',3],['LEPPA',4]];
    const parsed = parseBerryGeneration(raw, entries);
    assert.deepEqual(parsed.thresholds.map(row => row.berryId), [0,1,4]);
    assert.ok(parsed.raw.includes('futureField'));
    assert.throws(() => parseBerryGeneration(raw.replace('BerryType.LEPPA', 'BerryType.UNKNOWN'), entries));
    assert.throws(() => parseBerryGeneration(raw.replace('rand < 4', 'rand < 1'), entries));
  });

  test('Starter candy prices: normalize literals and preserve unknown fields', () => {
    const source = 'const allStarterCandyCosts: readonly StarterCandyCosts[] = [{passive: 40, costReduction: [25,60], eggCosts: [30,15], eggCostReductionThresholds: [20], futureField: 7},];';
    const parsed = parseStarterCandyPriceTable(source);
    assert.deepEqual(parsed.entries[0].costReduction, [25, 60]);
    assert.equal(parsed.entries[0].raw.futureField, 7);
    assert.throws(() => parseStarterCandyPriceTable(source.replace('[25,60]', '[25]')));
    assert.throws(() => parseStarterCandyPriceTable(source.replace('40', 'getPrice()')));
  });

  test('Rival pools: preserve grouped choices and unknown metadata; reject broken species', () => {
    const fixture = `
      const SLOT_1_FIGHT_1 = [SpeciesId.BULBASAUR, [SpeciesId.CHARMANDER, SpeciesId.SQUIRTLE]];
      function forceRivalBirdAbility(pokemon: EnemyPokemon): void {
        switch (pokemon.species.speciesId) { case SpeciesId.HOOTHOOT: { pokemon.abilityIndex = 2; break; } }
      }
      export const RIVAL_1_POOL: RivalPoolConfig = [
        { pool: SLOT_1_FIGHT_1, postProcess: forceRivalStarterTraits, futureField: 7 },
      ];`;
    const catalog = { getId: symbol => ['BULBASAUR', 'CHARMANDER', 'SQUIRTLE', 'HOOTHOOT'].includes(symbol) ? 1 : null };
    const slots = parseRivalPartyConfiguration(fixture, catalog).get('RIVAL_1_POOL');
    assert.deepEqual(slots[0].species, ['bulbasaur', ['charmander', 'squirtle']]);
    assert.ok(slots[0].raw.includes('futureField: 7'));
    assert.throws(() => parseRivalPartyConfiguration(fixture.replace('SpeciesId.SQUIRTLE', 'SpeciesId.UNKNOWN'), catalog), /Invalid rival species/);
    assert.throws(() => parseRivalPartyConfiguration(fixture.replace('pool: SLOT_1_FIGHT_1', 'pool: SLOT_9_FINAL'), catalog), /Missing rival slot pool/);
  });

  test('Content migration: pinned upstream content completes a deterministic first-run presentation flow', async () => {
    const importer = new PokerogueImporter(new PinnedLocalRepository(process.cwd()));
    const imported = await importer.importPlayableCanonicalContent();
    const repeatedImport = await importer.importPlayableCanonicalContent();
    assert.strictEqual(repeatedImport.importReport.contentHash, imported.importReport.contentHash, 'same pins/import/normalization produce the same canonical hash');
    const rival = imported.canonicalContent.collections.trainers.find(trainer => trainer.id === 'rival');
    assert.deepEqual(rival.trainerRules.rivalPartySlots.map(slot => slot.species.length), [27, 9]);
    assert.ok(rival.extensions.upstreamRivalConfig.raw.includes('function forceRivalBirdAbility'));
    assert.ok(rival.extensions.normalizedConfigFields.includes('rivalPartySlots'));
    const bird = rival.trainerRules.rivalPartySlots[1];
    assert.equal(bird.postProcessPolicy.forcedAbilities.find(row => row.speciesId === 'hoothoot').abilityIndex, 2);
    assert.equal(bird.postProcessPolicy.forcedAbilities.find(row => row.speciesId === 'wattrel').abilityIndex, 1);
    assert.equal(bird.provenance.sourcePath, 'src/data/trainers/rival-party-config.ts');
    assert.match(bird.provenance.sourceHash, /^[a-f0-9]{64}$/);
    assert.ok(bird.algorithmProvenance.raw.includes('CHOSEN_RIVAL_ROLLS'));
    assert.deepEqual(bird.referenceSpecies.slice(0, 2), ['pidgeot', 'noctowl']);
    const berry = imported.canonicalContent.extensions.berryGeneration;
    assert.deepEqual(berry.thresholds.map(row => [row.upperExclusive, row.berryId]), [[2,0],[4,1],[6,10]]);
    assert.equal(berry.entries.find(row => row.symbol === 'LEPPA').id, 10);
    assert.equal(berry.rollRange, 12);
    assert.equal(berry.fallbackExcludedCount, 3);
    assert.equal(berry.fallbackOffset, 2);
    assert.equal(berry.provenance.sourceSymbol, 'modifierTypeInitObj.BERRY');
    assert.equal(berry.enumProvenance.sourcePath, 'src/enums/berry-type.ts');
    assert.equal(berry.enumProvenance.revision, imported.canonicalContent.sourceSnapshot.revision);
    assert.match(berry.enumProvenance.sourceHash, /^[a-f0-9]{64}$/);
    assert.ok(berry.raw.includes('pregenArgs'));
    assert.equal(berry.effects.entries.length, 11);
    assert.ok(berry.effects.entries.every(row => row.resolved));
    assert.equal(berry.effects.provenance.sourcePath, 'src/data/berry.ts');
    assert.equal(berry.phase.provenance.sourceSymbol, 'BerryPhase.eatBerries');
    assert.match(berry.effects.provenance.sourceHash, /^[a-f0-9]{64}$/);
    for (const row of berry.effects.entries) {
      const localized = imported.canonicalContent.collections.locales.find(entry => entry.locale === 'en' &&
        entry.namespace === 'berry' && entry.id === row.symbol.toLowerCase());
      assert.ok(localized?.value?.name, `Missing Berry name ${row.symbol}`);
      assert.ok(localized?.value?.effect, `Missing Berry effect ${row.symbol}`);
    }

    const friendship = imported.canonicalContent.extensions.pokemonFriendshipRules;
    assert.deepStrictEqual(Object.fromEntries(Object.entries(friendship).map(([key, rule]) => [key, rule.value])),
      { battleGain: 3, rareCandyGain: 6, faintLoss: 5, rareCandyCap: 200 });
    assert.strictEqual(friendship.faintLoss.provenance.sourcePath, 'src/data/balance/starters.ts');
    assert.strictEqual(friendship.faintLoss.provenance.sourceSymbol, 'FRIENDSHIP_LOSS_FROM_FAINT');
    assert.strictEqual(friendship.faintLoss.provenance.revision, imported.canonicalContent.sourceSnapshot.revision);
    assert.match(friendship.faintLoss.provenance.sourceHash, /^[a-f0-9]{64}$/);
    const candyRules = imported.canonicalContent.extensions.starterCandyRules;
    assert.deepStrictEqual(candyRules.prices.entries.map(row => [row.cost, row.passive, ...row.costReduction]),
      [[1,40,25,60],[2,40,25,60],[3,35,20,50],[4,30,15,40],[5,25,12,35],
       [6,20,10,30],[7,15,8,20],[8,10,5,15],[9,10,5,15],[10,10,5,15]]);
    assert.deepStrictEqual(candyRules.prices.entries[0].eggCosts, [30,27,22,15]);
    assert.deepStrictEqual(candyRules.prices.entries[9].eggCostReductionThresholds, [8,16,32]);
    assert.strictEqual(candyRules.prices.provenance.sourceSymbol, 'allStarterCandyCosts');
    assert.strictEqual(candyRules.prices.provenance.sourcePath, 'src/data/balance/starters.ts');
    assert.strictEqual(candyRules.prices.provenance.revision, imported.canonicalContent.sourceSnapshot.revision);
    assert.match(candyRules.prices.provenance.sourceHash, /^[a-f0-9]{64}$/);
    assert.ok(candyRules.prices.raw.includes('eggCostReductionThresholds'));
    assert.strictEqual(candyRules.maxCandyCount.value, 9999);
    assert.strictEqual(candyRules.maxCandyCount.provenance.sourcePath, 'src/constants/game-constants.ts');
    assert.strictEqual(candyRules.classicMultiplier.value, 3);
    assert.deepStrictEqual(candyRules.friendshipCaps.entries.map(entry => [entry.cost, entry.value]),
      [[1,25],[2,50],[3,75],[4,100],[5,150],[6,200],[7,300],[8,450],[9,450],[10,600]]);
    assert.strictEqual(candyRules.friendshipCaps.fallback, 600);
    assert.strictEqual(candyRules.friendshipCaps.provenance.sourceSymbol, 'getStarterValueFriendshipCap');
    assert.match(candyRules.friendshipCaps.provenance.sourceHash, /^[a-f0-9]{64}$/);
    const fixed = imported.canonicalContent.extensions.fixedEnemyMovesets;
    assert.strictEqual(fixed.provenance.sourcePath, 'src/field/pokemon.ts');
    assert.strictEqual(fixed.provenance.sourceSymbol, 'EnemyPokemon.generateAndPopulateMoveset:ETERNATUS');
    assert.deepStrictEqual(fixed.entries.find(entry => entry.formIndex === 0).moves.map(move => move.moveId), [795, 188, 53, 322]);
    const secondPhase = fixed.entries.find(entry => entry.formIndex === 1);
    assert.deepStrictEqual(secondPhase.moves.map(move => move.moveId), [744, 440, 53, 105]);
    assert.strictEqual(secondPhase.moves[3].ppUp, -4);
    assert.ok(fixed.raw.includes('Challenges.INVERSE_BATTLE'));
    const pikachu = imported.species.find(species => species.id === 'pikachu');
    assert.strictEqual(pikachu.baseTotal, 320, 'explicit upstream baseTotal is normalized as canonical data');
    assert.strictEqual(pikachu.growthRate, 'MEDIUM_FAST', 'growth-rate identifier is normalized without a gameplay port');
    const pikachuGigantamax = imported.canonicalContent.collections.forms.find(form => form.speciesId === 'pikachu' && form.formKey === 'GIGANTAMAX');
    assert.ok(pikachuGigantamax.levelMoves.some(move => move.move === 'zippy_zap' && move.level === 20), 'pinned form-specific level moves reach the matching canonical form record');
    const indexedForms = imported.canonicalContent.collections.forms.filter(form => form.speciesId === 'pikachu');
    assert.deepStrictEqual(indexedForms.map(form => form.extensions.upstreamFormIndex),
      indexedForms.map((_, index) => index), 'form indexes preserve source constructor order');

    assert.deepStrictEqual(pikachu.rarity, { legendary: false, subLegendary: false, mythical: false }, 'pinned PokemonSpecies constructor defaults omitted rarity flags to false');
    const legendary = imported.species.find(species => species.rarity.legendary === true);
    assert.ok(legendary, 'explicit upstream legendary classification is retained');
    assert.strictEqual(imported.canonicalContent.extensions.biomePoolReferenceAudit.status, 'COMPLETE_AND_VALIDATED', 'complete production import validates biome species references');
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
    // Read fields by name; attribute ranges added to the schema retain coverage.
    const generatedRow = (model, key, name) => {
      const declaration = header.match(new RegExp(`struct ${model} \\{([^}]+)\\}`));
      assert.ok(declaration, `${model} declaration exists`);
      const fields = declaration[1].split(';').filter(field => field.trim())
        .map(field => field.trim().split(/\s+/).at(-1));
      const line = header.split('\n').find(line => line.includes(`, "${key}", "${name}"`));
      assert.ok(line, `real ${key} row exists`);
      const values = line.slice(line.indexOf('{') + 1, line.lastIndexOf('}'))
        .match(/"(?:[^"\\]|\\.)*"|[^,]+/g).map(value => value.trim());
      assert.strictEqual(values.length, fields.length, `${model} row matches its schema`);
      return Object.fromEntries(fields.map((field, index) => [field, values[index]]));
    };
    const row = generatedRow('Species', 'bulbasaur', 'Bulbasaur');
    assert.deepStrictEqual(Object.fromEntries(['dex','malePercentTenths','baseTotal','hp','atk','def','spatk','spdef','speed','ability1','ability2','abilityHidden','legendary','subLegendary','mythical'].map(field => [field, Number(row[field])])),
      { dex:1, malePercentTenths:875, baseTotal:318, hp:45, atk:49, def:49, spatk:65, spdef:65, speed:45, ability1:65, ability2:65, abilityHidden:34, legendary:0, subLegendary:0, mythical:0 });
    assert.strictEqual(row.growthRate, '"MEDIUM_SLOW"');
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
    const expectMove = (key, name, expected) => {
      const actual = generatedRow('Move', key, name);
      assert.deepStrictEqual(Object.fromEntries(Object.keys(expected).map(field => [field, actual[field]])), expected);
    };
    expectMove('tackle', 'Tackle', { id:'33', category:'0', power:'40', accuracy:'100', pp:'35', priority:'0', upstreamChance:'-1', generation:'1', upstreamFlags:'0', multiHitType:'0', attributeCount:'0', type:'"NORMAL"', target:'"NEAR_OTHER"' });
    expectMove('bide', 'Bide', { id:'117', category:'0', power:'-1', accuracy:'-1', pp:'10', priority:'1', upstreamChance:'-1', generation:'1', upstreamFlags:'MoveIsUnimplemented | MoveIsStabBlacklisted', multiHitType:'0', attributeCount:'0' });
    expectMove('memento', 'Memento', { id:'262', category:'2', power:'-1', accuracy:'100', pp:'10', priority:'0', upstreamChance:'-1', generation:'3', upstreamFlags:'MoveHasSacrificialAttrOnHit | MoveHasSacrificialAttr', multiHitType:'0', attributeCount:'2' });
    assert.ok(header.includes('uint16_t upstreamFlags'), 'native move metadata exposes upstream move-generation and power flags');
    assert.match(header, /MoveIsStabBlacklisted = 2048/, 'native move metadata preserves pinned forced-STAB blacklist');
    expectMove('double_slap', 'Double Slap', { id:'3', category:'0', power:'15', accuracy:'85', pp:'10', priority:'0', upstreamChance:'-1', generation:'1', upstreamFlags:'MoveHasMultiHit', multiHitType:'1', attributeCount:'1' });
    assert.ok(header.includes('int8_t level; uint16_t moveId;'), 'native learnset retains upstream signed sentinel levels');
    assert.ok(header.includes('levelMovesFor(const Form& form)'), 'native runtime exposes form-specific learnset ranges');
    assert.ok(header.includes('kMoveStatusEffects[]'), 'real StatusEffectAttr metadata reaches C++ tables');
    const thunderWave = canonical.collections.moves.find(move => move.id === 'thunder_wave');
    assert.ok(thunderWave.extensions.upstreamRawRecord.value.includes('StatusEffectAttr, StatusEffect.PARALYSIS'));
    assert.ok(header.includes(`{${thunderWave.moveId}, "PARALYSIS", 3, false, true,`),
      'Thunder Wave preserves the pinned constant effect and constructor selfTarget default');
    assert.ok(header.includes('kFormChangeReferences[]'), 'form-change targets reach native capture rules');
    const venusaurChanges = canonical.collections.species.find(species => species.id === 'venusaur')
      .extensions.upstreamFormChanges;
    assert.ok(venusaurChanges.some(change => change.formKey === 'MEGA' && change.raw.includes('SpeciesFormChangeItemTrigger')),
      'real Venusaur transformation retains target identity and unported trigger source');
    assert.ok(header.includes('kFormPermissions[]'), 'form permissions reach generated C++ data');
    for (const form of canonical.collections.forms) {
      for (const key of ['isUnobtainable', 'isStarterSelectable']) {
        assert.ok(typeof form.extensions[key] === 'boolean' || form.extensions[key] === null,
          `${form.id}: explicit upstream form permission metadata ${key}`);
      }
    }
    assert.strictEqual(canonical.collections.forms.find(form => form.id === 'pikachu:gigantamax')
      .extensions.isStarterSelectable, false, 'Gigantamax is not granted starter selection by observation');
    const gigantamaxRow = header.split(/\r?\n/).find(line => line.includes('{"pikachu:gigantamax"') && line.includes('"ELECTRIC"'));
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
