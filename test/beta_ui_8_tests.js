import assert from 'assert';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

import { ComponentRegistry } from '../public/js/components/ComponentRegistry.js';
import { SceneModel } from '../public/js/core/SceneModel.js';
import { SceneLibrary } from '../public/js/core/SceneLibrary.js';
import { Effect, EffectStack, BlendModes } from '../public/js/core/EffectModel.js';
import { TextNode } from '../public/js/components/TextNode.js';
import { ShapeNode, ShapeTypes } from '../public/js/components/ShapeNode.js';
import { ProjectDocument, RecentProjectsManager } from '../public/js/core/ProjectDocument.js';
import { PresetManager } from '../public/js/core/PresetManager.js';
import { DependencyGraph } from '../public/js/core/DependencyGraph.js';
import { GlobalSearch } from '../public/js/editor/GlobalSearch.js';
import { CommandPalette } from '../public/js/editor/CommandPalette.js';
import { KeyboardShortcuts } from '../public/js/editor/KeyboardShortcuts.js';
import { CompositionNavigator } from '../public/js/editor/CompositionNavigator.js';
import { SceneTabs } from '../public/js/editor/SceneTabs.js';
import { ProjectValidator } from '../public/js/generator/ProjectValidator.js';
import { ExportReport } from '../public/js/generator/ExportReport.js';
import { BatchExporter } from '../public/js/generator/BatchExporter.js';
import { BuildOutputPanel } from '../public/js/editor/BuildOutputPanel.js';
import { CanvasRenderer } from '../public/js/editor/CanvasRenderer.js';
import { AnimationTrack } from '../public/js/animation/AnimationTrack.js';
import { TimelineEvaluator } from '../public/js/animation/TimelineEvaluator.js';
import { SceneCppExporter } from '../public/js/generator/SceneCppExporter.js';
import { BattleEngine } from '../public/js/battle/BattleEngine.js';
import { BattleSession } from '../public/js/battle/BattleSession.js';
import { BattleState } from '../public/js/battle/BattleState.js';
import { CanonicalContent, CanonicalSourceType, OverrideSet, Provenance, RuntimeContent, SourceSnapshot } from '../public/js/data/CanonicalDataContract.js';
import { PokerogueManifest } from '../public/js/data/PokerogueManifest.js';
import { DataManager } from '../public/js/data/DataManager.js';
import { PokerogueImporter } from '../public/js/data/PokerogueImporter.js';
import { PokerogueRepository } from '../public/js/data/PokerogueRepository.js';
import { GameModeDefinition, GameModeRegistry } from '../public/js/game/GameMode.js';
import { BiomeDefinition, MapDefinition, MapEdge, MapNode, ProgressionContentRegistry, RouteDefinition, WaveDefinition } from '../public/js/game/ProgressionContent.js';
import { selectRelevantSubgraph, serializeSemanticBrainMap } from '../scripts/semantic_brain_map_query.mjs';
import { progressionFixtureProvenance } from './fixtures/progressionContracts.js';
import { registerBetaUI9ATests } from './beta_ui_9a_tests.js';
import { registerBetaUI9CRngTests } from './beta_ui_9c_rng_tests.js';
import { registerBetaUI9DTests } from './beta_ui_9d_tests.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

export function registerBetaUI8Tests(test) {

  test('BETA-UI-8A: canonical snapshot, determinism, overrides and compatibility', () => {
    const snapshotA = new SourceSnapshot({ repository: 'https://github.com/pagefaultgames/pokerogue', revision: '8555c08c823b856cbec4eb99ca84ea52a955836d', sourceType: 'UPSTREAM', importTimestamp: '2026-01-01' });
    const snapshotB = new SourceSnapshot({ sourceType: 'UPSTREAM', revision: '8555c08c823b856cbec4eb99ca84ea52a955836d', repository: 'https://github.com/pagefaultgames/pokerogue', importTimestamp: '2026-02-01' });
    assert.strictEqual(snapshotA.identity(), snapshotB.identity());
    assert.strictEqual(PokerogueManifest.computeHash({ b: 2, a: 1 }), PokerogueManifest.computeHash({ a: 1, b: 2 }));
    const content = new CanonicalContent({
      sourceSnapshot: snapshotA,
      provenance: new Provenance({ sourceRepository: snapshotA.repository, sourceRevision: snapshotA.revision, sourcePath: 'src/data/example.ts', sourceType: 'UPSTREAM' }),
      collections: { species: [{ id: 'testmon', presentation: { scale: 1 } }] }
    });
    const originalHash = content.hash();
    const runtime = new RuntimeContent({ canonicalContent: content, overrideSet: new OverrideSet({ overrides: [{ target: 'species:testmon', path: 'presentation.scale', value: 0.5, scope: 'presentation' }] }), runtimeCompatibility: { minimumRuntimeVersion: '1.2.0' } });
    const first = runtime.resolve();
    const second = runtime.resolve();
    assert.strictEqual(first.collections.species[0].presentation.scale, 0.5);
    assert.strictEqual(content.collections.species[0].presentation.scale, 1);
    assert.strictEqual(PokerogueManifest.stableStringify(first), PokerogueManifest.stableStringify(second));
    assert.strictEqual(content.hash(), originalHash);
    assert.deepStrictEqual(runtime.validateCompatibility({ runtimeVersion: '1.1.9' }).some(error => error.includes('below minimum')), true);
    assert.ok(new CanonicalContent({ ...content, schemaVersion: '9.0.0' }).validate().some(error => error.includes('Unsupported canonical schemaVersion')));
  });

  test('BETA-UI-8A: fixture source cannot be promoted to production', async () => {
    const manager = new DataManager();
    const initialCount = manager.getAllSpecies().length;
    await assert.rejects(() => manager.importUpstream({ importVerticalSlice: async () => ({ sourceType: CanonicalSourceType.TEST_FIXTURE, species: [], moves: [], abilities: [], canonicalContent: new CanonicalContent({ sourceSnapshot: { repository: 'local:test/fixtures/fallbackVerticalSlice.js', revision: 'fixture-1', sourceType: 'TEST_FIXTURE' }, provenance: { sourceRepository: 'local:test/fixtures/fallbackVerticalSlice.js', sourceRevision: 'fixture-1', sourcePath: 'test/fixtures/fallbackVerticalSlice.js', sourceType: 'TEST_FIXTURE' } }) }) }), /not verified UPSTREAM/);
    assert.strictEqual(manager.getAllSpecies().length, initialCount);
  });

  test('BETA-UI-8A: importer preserves raw upstream records and labels offline content', async () => {
    const importer = new PokerogueImporter(null);
    const dataset = await importer.importVerticalSlice(null);
    assert.strictEqual(dataset.sourceType, CanonicalSourceType.TEST_FIXTURE);
    assert.ok(dataset.species[0].extensions.upstreamRawRecord.value.includes('speciesId: 25'));
    assert.ok(dataset.species[0].source.sourcePath.startsWith('test/fixtures/'));
    const parsed = new PokerogueImporter().parseSpeciesFromGeneration(`const generationOneSpeciesData = { [SpeciesId.TESTMON]: { speciesId: 999, name: 'Testmon', type1: Type.NORMAL, baseStats: [1, 2, 3, 4, 5, 6], evolutions: [new SpeciesEvolution({ speciesId: SpeciesId.TESTEVOLVE, level: 16, condition: { key: EvoCondKey.FRIENDSHIP, value: 120 }, evoDelay: [18, 20, 22] })], futureUpstreamField: { retained: true } } };`, ['TESTMON']);
    assert.ok(parsed[0].extensions.upstreamRawRecord.value.includes('futureUpstreamField'));
    assert.equal(parsed[0].evolutions[0].targetSpeciesId, 'testevolve');
    assert.equal(parsed[0].evolutions[0].level, 16);
    assert.deepEqual(parsed[0].evolutions[0].evoLevelThreshold, { strong: 18, normal: 20, wild: 22 });
    assert.match(parsed[0].evolutions[0].condition, /FRIENDSHIP/);
    assert.match(parsed[0].evolutions[0].extensions.upstreamRawRecord.value, /evoDelay/);
  });

  test('BETA-UI-8B: pinned PokéRogue modes import into canonical policy registry', async () => {
    const imported = await new PokerogueImporter(new PokerogueRepository()).importGameModes();
    const expected = ['classic', 'endless', 'spliced_endless', 'daily', 'challenge'].sort();
    assert.deepStrictEqual(imported.gameModes.map(mode => mode.id).sort(), expected);
    assert.strictEqual(imported.sourceType, 'UPSTREAM');
    assert.strictEqual(imported.canonicalContent.sourceSnapshot.revision, '8555c08c823b856cbec4eb99ca84ea52a955836d');
    assert.ok(imported.gameModes.every(mode => mode.provenance.sourceType === 'UPSTREAM' && mode.provenance.sourcePath === 'src/game-mode.ts'));

    const registry = new GameModeRegistry();
    assert.strictEqual(registry.loadCanonicalContent(imported.canonicalContent), 5);
    const runtimeRegistry = new GameModeRegistry();
    assert.strictEqual(runtimeRegistry.loadRuntimeContent(new RuntimeContent({ canonicalContent: imported.canonicalContent }), { runtimeVersion: '1.0.0' }), 5);
    const classic = registry.require('classic');
    const endless = registry.require('endless');
    const spliced = registry.require('spliced_endless');
    const daily = registry.require('daily');
    const challenge = registry.require('challenge');
    assert.strictEqual(classic.supports('trainerBattles'), true);
    assert.strictEqual(classic.getMaxWave(), 200);
    assert.strictEqual(endless.getMaxWave(), null, 'Endless does not get a fake structural maximum');
    assert.strictEqual(endless.supports('shortBiomes'), true);
    assert.strictEqual(spliced.supports('splicedOnly'), true);
    assert.strictEqual(daily.getMaxWave(), 50);
    assert.strictEqual(daily.definition.contextRequirements.seed, true);
    assert.strictEqual(daily.supports('shop'), false);
    assert.strictEqual(challenge.supports('challengeRules'), true);
    assert.strictEqual(classic.supports('map'), null, 'unmodeled upstream map policy stays explicitly unknown');
    assert.strictEqual(registry.hash(), new GameModeRegistry(imported.gameModes).hash());

    const fixtureProvenance = { sourceRepository: 'local:test/fixtures/game-mode-contract.js', sourceRevision: 'fixture-1', sourcePath: 'test/fixtures/game-mode-contract.js', sourceType: 'TEST_FIXTURE' };
    const capabilityRegistry = new GameModeRegistry([
      new GameModeDefinition({ id: 'map_enabled', displayNameKey: 'fixture.mapEnabled', capabilities: { map: true }, provenance: fixtureProvenance }),
      new GameModeDefinition({ id: 'map_disabled', displayNameKey: 'fixture.mapDisabled', capabilities: { map: false }, provenance: fixtureProvenance })
    ]);
    assert.strictEqual(capabilityRegistry.require('map_enabled').supports('map'), true);
    assert.strictEqual(capabilityRegistry.require('map_disabled').supports('map'), false);
    assert.strictEqual(capabilityRegistry.hash(), new GameModeRegistry(capabilityRegistry.list()).hash());
    assert.ok(new GameModeDefinition({ id: 'invalid_mode', displayNameKey: 'fixture.invalid', rules: { maxWave: 0 }, provenance: fixtureProvenance }).validate().some(error => error.includes('rules.maxWave')));
    assert.throws(() => new GameModeRegistry([capabilityRegistry.list()[0], capabilityRegistry.list()[0]]), /Duplicate GameMode id/);
    const requiredExtension = new GameModeDefinition({ id: 'extension_mode', displayNameKey: 'fixture.extension', extensions: { required: ['future.mode-rule'] }, provenance: fixtureProvenance });
    assert.ok(requiredExtension.validate().some(error => error.includes('Unknown required GameMode extension')));

    const source = fs.readFileSync(path.join(__dirname, '../public/js/game/GameMode.js'), 'utf8');
    assert.doesNotMatch(source, /SceneModel|CanvasRenderer|TimelineUI|PokemonSpriteNode/);
  });

  test('BETA-UI-8B: semantic map returns a deterministic focused GameMode subgraph', () => {
    const graphPath = path.join(__dirname, '../docs/semantic-brain-map.json');
    const graphText = fs.readFileSync(graphPath, 'utf8');
    const graph = JSON.parse(graphText);
    const selected = selectRelevantSubgraph(graph, { query: 'GameMode', depth: 1 });
    const repeated = selectRelevantSubgraph(graph, { query: 'GameMode', depth: 1 });
    assert.deepStrictEqual(selected.nodes.map(node => node.id), repeated.nodes.map(node => node.id));
    assert.ok(selected.contextBudget.primary.includes('domain.game-mode-registry'));
    assert.ok(selected.contextBudget.reference.includes('source.pokerogue-game-mode'));
    assert.ok(selected.nodes.some(node => node.id === 'test.beta-ui-8'));
    assert.ok(!selected.nodes.some(node => node.id === 'presentation.canvas-renderer'));
    assert.ok(graph.violations.some(item => item.id === 'violation.flow-hardcodes-wave-progression'));
    assert.ok(graph.nodes.every(node => node.responsibility && node.source && node.type && node.layer));
    assert.deepStrictEqual(graph.nodes.map(node => node.id), graph.nodes.map(node => node.id).sort());
    const validNodeIds = new Set(graph.nodes.map(node => node.id));
    assert.ok(graph.edges.every(edge => validNodeIds.has(edge.from) && validNodeIds.has(edge.to)));
    assert.strictEqual(graphText, serializeSemanticBrainMap(graph));

    const biomeContext = selectRelevantSubgraph(graph, { query: 'modify biome progression', depth: 0 });
    assert.ok(biomeContext.contextBudget.primary.includes('domain.biome-definition'));
    assert.ok(biomeContext.contextBudget.primary.includes('domain.game-mode-policy'));
    assert.ok(biomeContext.contextBudget.primary.includes('domain.wave-definition'));
    assert.ok(biomeContext.contextBudget.primary.includes('domain.biome-resolver-target'));
    assert.ok(biomeContext.contextBudget.secondary.includes('test.beta-ui-8c'));
    assert.ok(biomeContext.contextBudget.reference.includes('source.pokerogue-biome-links'));
    assert.ok(!biomeContext.nodes.some(node => ['presentation.canvas-renderer', 'presentation.timeline', 'runtime.scene-player'].includes(node.id)));

    const battleStateContext = selectRelevantSubgraph(graph, { query: 'initialize native Pokémon battle state', depth: 0 });
    assert.ok(battleStateContext.contextBudget.primary.includes('domain.pokemon-battle-state'));
    assert.ok(battleStateContext.contextBudget.primary.includes('runtime.pokemon-battle-state'));
    assert.ok(battleStateContext.contextBudget.reference.includes('source.pokerogue-pokemon-instance-rules'));
    assert.ok(battleStateContext.contextBudget.secondary.includes('test.beta-ui-9d'));
    assert.ok(!battleStateContext.nodes.some(node => ['presentation.canvas-renderer', 'presentation.timeline', 'runtime.scene-player'].includes(node.id)));
  });

  test('BETA-UI-8C: pinned biome import becomes canonical biome and route graph', async () => {
    const imported = await new PokerogueImporter(new PokerogueRepository()).importBiomes();
    assert.strictEqual(imported.sourceType, CanonicalSourceType.UPSTREAM);
    assert.strictEqual(imported.sourceSnapshot.revision, '8555c08c823b856cbec4eb99ca84ea52a955836d');
    assert.ok(imported.biomes.length > 0);
    assert.ok(imported.routes.length > imported.biomes.length);
    assert.deepStrictEqual(imported.canonicalContent.collections.waves, []);
    assert.deepStrictEqual(imported.canonicalContent.collections.maps, []);
    assert.ok(imported.biomes.every(item => item.provenance.sourceType === 'UPSTREAM' && item.provenance.sourcePath.startsWith('src/data/balance/biomes/') && item.provenance.sourceHash));
    assert.ok(imported.canonicalContent.sourceSnapshot.sources.some(source => source.repository === 'pokerogue-locales' && source.sourcePath === 'en/biomes.json'));
    assert.ok(imported.biomes.every(item => item.extensions.upstreamLocaleRef?.sha256 && item.extensions.upstreamEnumRef?.sha256));
    assert.ok(imported.biomes.some(item => item.extensions.upstreamRawRecord.value.includes('const pokemonPool')));
    const plains = imported.biomes.find(item => item.id === 'plains');
    assert.deepStrictEqual(plains.encounterPools.common.dawn, ['sentret', 'yungoos', 'skwovet']);
    assert.deepStrictEqual(plains.encounterPools.common.all, ['zigzagoon', 'bidoof', 'lechonk']);
    assert.deepStrictEqual(plains.trainerPools.common, ['breeder', 'twins']);
    assert.strictEqual(plains.provenance.sourceSymbol, 'plainsBiome');
    assert.ok(Object.values(plains.encounterPools).every(tier => ['dawn', 'day', 'dusk', 'night', 'all'].every(time => Array.isArray(tier[time]))));
    assert.ok(imported.routes.every(item => imported.biomes.some(biome => biome.id === item.from) && imported.biomes.some(biome => biome.id === item.to)));
    const registry = new ProgressionContentRegistry();
    assert.strictEqual(registry.loadCanonicalContent(imported.canonicalContent), imported.biomes.length + imported.routes.length);
    const repeated = new ProgressionContentRegistry();
    repeated.loadCanonicalContent(imported.canonicalContent);
    assert.strictEqual(registry.hash(), repeated.hash());
    const runtimeRegistry = new ProgressionContentRegistry();
    assert.strictEqual(runtimeRegistry.loadRuntimeContent(new RuntimeContent({ canonicalContent: imported.canonicalContent }), { runtimeVersion: '1.0.0' }), imported.biomes.length + imported.routes.length);
  });

  test('BETA-UI-8C: progression contracts preserve data, provenance and deterministic serialization', () => {
    const source = progressionFixtureProvenance;
    const wave = new WaveDefinition({ id: 'classic.wave.1', waveNumber: 1, modeId: 'classic', biomeId: 'plains', encounterPool: { id: 'pool_ref' }, reward: { policyRef: 'future.reward-policy' }, sceneBinding: { template: 'battle' }, provenance: source, extensions: { upstreamFutureField: { keep: true } } });
    const sameWave = new WaveDefinition(JSON.parse(wave.serialize()));
    assert.strictEqual(wave.serialize(), sameWave.serialize());
    assert.strictEqual(wave.provenance.sourceType, CanonicalSourceType.TEST_FIXTURE);
    assert.deepStrictEqual(wave.extensions.upstreamFutureField, { keep: true });
    assert.strictEqual(wave.modeId, 'classic');
    assert.strictEqual(wave.biomeId, 'plains');
    assert.doesNotMatch(wave.serialize(), /executeBattle|choosePokemon|renderScene|giveReward/);

    const biome = new BiomeDefinition({ id: 'plains', localizedName: { namespace: 'biomes', key: 'plains' }, visualTemplate: 'terrain.generic', routes: ['route.plains.forest'], provenance: source, extensions: { future: 'retained' } });
    assert.strictEqual(biome.serialize(), new BiomeDefinition(JSON.parse(biome.serialize())).serialize());
    assert.deepStrictEqual(biome.extensions, { future: 'retained' });
    const route = new RouteDefinition({ id: 'route.plains.forest', from: 'plains', to: 'forest', weight: 2, provenance: source });
    const classicMap = new MapDefinition({ id: 'classic.route-graph', modeId: 'classic', nodes: [new MapNode({ id: 'start', type: 'battle', payload: { contentRef: 'wave:classic.wave.1' }, connections: ['forest'], provenance: source }), new MapNode({ id: 'forest', type: 'transition', provenance: source })], edges: [new MapEdge({ from: 'start', to: 'forest', weight: 2 })], provenance: source });
    const registry = new ProgressionContentRegistry();
    registry.loadCanonicalContent(new CanonicalContent({ sourceSnapshot: { repository: source.sourceRepository, revision: source.sourceRevision, sourceType: source.sourceType }, provenance: source, collections: { waves: [wave], biomes: [biome, new BiomeDefinition({ id: 'forest', provenance: source })], maps: [classicMap], routes: [route] } }));
    assert.throws(() => new ProgressionContentRegistry().loadCanonicalContent(new CanonicalContent({ schemaVersion: '9.0.0', sourceSnapshot: { repository: source.sourceRepository, revision: source.sourceRevision, sourceType: source.sourceType }, provenance: source, collections: { waves: [wave] } })), /Unsupported canonical schemaVersion/);
    assert.strictEqual(classicMap.validate().length, 0);
    assert.strictEqual(classicMap.hash(), new MapDefinition(JSON.parse(classicMap.serialize())).hash());
    assert.ok(new MapDefinition({ id: 'invalid.graph', nodes: [new MapNode({ id: 'a', type: 'event' })], edges: [{ from: 'a', to: 'missing' }], provenance: source }).validate().some(error => error.includes('unknown target node')));
    const cyclic = new MapDefinition({ id: 'cycle.graph', nodes: [{ id: 'a', type: 'other' }, { id: 'b', type: 'other' }], edges: [{ from: 'a', to: 'b' }, { from: 'b', to: 'a' }], provenance: source });
    assert.deepStrictEqual(cyclic.validate(), [], 'cycles are valid graph topology; no DAG assumption is imposed');
    assert.ok(new WaveDefinition({ id: 'invalid.wave', waveNumber: 0, provenance: source }).validate().some(error => error.includes('waveNumber')));
    assert.ok(new RouteDefinition({ id: 'bad.route', from: 'plains', to: 'nope', weight: -1, provenance: source }).validate().some(error => error.includes('weight')));
  });

  test('BETA-UI-8C: all pinned GameModes accept declarative progression references', async () => {
    const imported = await new PokerogueImporter(new PokerogueRepository()).importCanonicalProgression();
    const source = progressionFixtureProvenance;
    const waves = imported.gameModes.map(mode => new WaveDefinition({ id: `${mode.id}.wave.1`, waveNumber: 1, modeId: mode.id, biomeId: 'plains', provenance: source }));
    const biomes = imported.biomes;
    const maps = [new MapDefinition({ id: 'classic.map', modeId: 'classic', nodes: [{ id: 'start', type: 'battle', payload: { contentRef: 'classic.wave.1' }, provenance: source }], edges: [], provenance: source })];
    const content = new CanonicalContent({ sourceSnapshot: imported.sourceSnapshot, provenance: imported.canonicalContent.provenance, collections: { ...imported.canonicalContent.collections, waves, biomes, maps } });
    assert.deepStrictEqual(waves.map(wave => wave.modeId).sort(), ['challenge', 'classic', 'daily', 'endless', 'spliced_endless']);
    const registry = new ProgressionContentRegistry();
    assert.strictEqual(registry.loadCanonicalContent(content), waves.length + biomes.length + maps.length + imported.routes.length);
    assert.deepStrictEqual(maps.map(map => map.modeId), ['classic']);
    assert.deepStrictEqual(waves.map(wave => wave.biomeId), Array(waves.length).fill('plains'));
    assert.strictEqual(new GameModeRegistry(imported.gameModes).list().length, waves.length);
  });

  test('BETA-UI-8.1: Project model', () => {
    const doc = new ProjectDocument({ name: 'ProductionProject' });
    assert.strictEqual(doc.schemaVersion, 1);
    assert.strictEqual(doc.name, 'ProductionProject');
    assert.strictEqual(doc.settings.targetFps, 60);
    assert.strictEqual(doc.getScenes().length, 1); // default MainScene

    const scene2 = doc.addScene({ id: 'Level1', name: 'Level 1 Scene' });
    assert.strictEqual(doc.getScenes().length, 2);
    assert.strictEqual(doc.getScene('Level1').name, 'Level 1 Scene');

    doc.setActiveScene('Level1');
    assert.strictEqual(doc.activeSceneId, 'Level1');
    assert.strictEqual(doc.getActiveScene().id, 'Level1');

    doc.removeScene('MainScene');
    assert.strictEqual(doc.getScenes().length, 1);

    // Cannot remove only remaining scene
    assert.throws(() => doc.removeScene('Level1'), /Cannot remove the only remaining scene/);
  });

  test('BETA-UI-8.2: Project save/load', () => {
    RecentProjectsManager.clearRecentProjects();
    const doc = ProjectDocument.newProject('SaveLoadProject');
    doc.addScene({ id: 'AuxScene', name: 'Aux Scene' });
    doc.templates.push({ id: 't1', name: 'Custom Template' });
    doc.assets.push({ id: 'a1', type: 'image', path: 'romfs:/test.t3x' });

    const saved = doc.saveProject();
    assert.strictEqual(saved.name, 'SaveLoadProject');
    assert.strictEqual(saved.scenes.length, 2);
    assert.strictEqual(saved.templates.length, 1);
    assert.strictEqual(saved.assets.length, 1);

    const loaded = ProjectDocument.loadProject(saved);
    assert.strictEqual(loaded.name, 'SaveLoadProject');
    assert.strictEqual(loaded.getScenes().length, 2);
    assert.strictEqual(loaded.templates.length, 1);
    assert.strictEqual(loaded.assets.length, 1);

    // Recent list recorded
    assert.ok(RecentProjectsManager.getRecentProjects().includes('SaveLoadProject'));

    // Corrupt JSON string throws descriptive diagnostics
    assert.throws(() => ProjectDocument.loadProject('{corrupt:json,'), /Corrupted JSON/);
    assert.throws(() => ProjectDocument.loadProject(null), /input data is null/);
  });

  test('BETA-UI-8.3: Project validation', () => {
    const cleanDoc = new ProjectDocument({ name: 'CleanProject' });
    const cleanResult = ProjectValidator.validate(cleanDoc);
    assert.strictEqual(cleanResult.valid, true);
    assert.strictEqual(cleanResult.errors.length, 0);

    // Broken project: missing asset reference, cycle in compositions, invalid dimension
    const brokenScene = new SceneModel({
      id: 'BrokenScene',
      name: 'Broken Scene',
      nodes: [
        {
          id: 'bad_image',
          type: 'Image',
          x: 0,
          y: 0,
          width: -10, // Invalid dimension
          height: 100,
          properties: { asset: 'missing_texture_asset' }
        }
      ],
      compositions: [
        { id: 'comp_a', nodes: [{ id: 'ref_b', type: 'Composition', properties: { compositionId: 'comp_b' } }] },
        { id: 'comp_b', nodes: [{ id: 'ref_a', type: 'Composition', properties: { compositionId: 'comp_a' } }] }
      ]
    });

    const brokenDoc = new ProjectDocument({
      name: 'BrokenProject',
      scenes: [brokenScene],
      assets: []
    });

    const brokenResult = ProjectValidator.validate(brokenDoc);
    assert.strictEqual(brokenResult.valid, false);
    assert.ok(brokenResult.errors.length > 0);

    // Check categorized structure
    assert.ok(Array.isArray(brokenResult.errors));
    assert.ok(Array.isArray(brokenResult.warnings));
    assert.ok(Array.isArray(brokenResult.info));

    // Must identify coordinates
    const assetErr = brokenResult.errors.find(e => e.message?.includes('missing_texture_asset') || e.code === 'MISSING_ASSET');
    assert.ok(assetErr, 'Should detect missing asset');
    assert.strictEqual(assetErr.scene, 'BrokenScene');
    assert.strictEqual(assetErr.node, 'bad_image');
  });

  test('BETA-UI-8.4: Global search', () => {
    const doc = new ProjectDocument({
      name: 'SearchProject',
      scenes: [
        {
          id: 'TitleScene',
          name: 'Title Screen',
          nodes: [
            { id: 'pikachu_hero', name: 'Pikachu Sprite', type: 'Text', properties: { text: 'Pikachu' } }
          ],
          clips: [
            { id: 'clip_intro_anim', name: 'Intro Animation Clip', startFrame: 0, durationFrames: 30 }
          ],
          markers: [
            { id: 'marker_audio_hit', frame: 15, label: 'Audio Hit Marker' }
          ],
          compositions: [
            { id: 'comp_hud_box', name: 'HUD Box Composition', nodes: [] }
          ]
        }
      ],
      assets: [
        { id: 'asset_bg_forest', name: 'Forest Background', type: 'image' }
      ]
    });

    const nodeResults = GlobalSearch.search(doc, 'Pikachu');
    assert.ok(nodeResults.some(r => r.type === 'node' && r.id === 'pikachu_hero'));

    const sceneResults = GlobalSearch.search(doc, 'Title');
    assert.ok(sceneResults.some(r => r.type === 'scene' && r.id === 'TitleScene'));

    const clipResults = GlobalSearch.search(doc, 'Intro');
    assert.ok(clipResults.some(r => r.type === 'clip' && r.id === 'clip_intro_anim'));

    const markerResults = GlobalSearch.search(doc, 'Audio');
    assert.ok(markerResults.some(r => r.type === 'marker' && r.id === 'marker_audio_hit'));

    const assetResults = GlobalSearch.search(doc, 'Forest');
    assert.ok(assetResults.some(r => r.type === 'asset' && r.id === 'asset_bg_forest'));
  });

  test('BETA-UI-8.5: Command palette', () => {
    const palette = new CommandPalette();
    let executedAction = null;

    palette.registerDefaults({
      onNewScene: () => { executedAction = 'New Scene'; },
      onSaveProject: () => { executedAction = 'Save Project'; },
      onExport: () => { executedAction = 'Export'; },
      onPlay: () => { executedAction = 'Play'; },
      onPause: () => { executedAction = 'Pause'; },
      onFitScreen: () => { executedAction = 'Fit Screen'; },
      onAddKeyframe: () => { executedAction = 'Add Keyframe'; },
      onCreateComposition: () => { executedAction = 'Create Composition'; }
    });

    const searchHits = palette.search('save');
    assert.ok(searchHits.some(c => c.label === 'Save Project'));

    const executed = palette.execute('save_project');
    assert.strictEqual(executed, true);
    assert.strictEqual(executedAction, 'Save Project');

    palette.execute('play');
    assert.strictEqual(executedAction, 'Play');
  });

  test('BETA-UI-8.6: Shortcut handling', () => {
    let triggered = null;
    const shortcuts = new KeyboardShortcuts({
      onPlayPause: () => { triggered = 'play_pause'; },
      onAddKeyframe: () => { triggered = 'add_keyframe'; },
      onFitScreen: () => { triggered = 'fit_screen'; },
      onDelete: () => { triggered = 'delete'; },
      onCopy: () => { triggered = 'copy'; },
      onPaste: () => { triggered = 'paste'; },
      onUndo: () => { triggered = 'undo'; },
      onRedo: () => { triggered = 'redo'; }
    });

    // Space -> play/pause
    shortcuts.handleKeyDown({ code: 'Space', target: { tagName: 'DIV' } });
    assert.strictEqual(triggered, 'play_pause');

    // K -> add keyframe
    shortcuts.handleKeyDown({ code: 'KeyK', key: 'k', target: { tagName: 'DIV' } });
    assert.strictEqual(triggered, 'add_keyframe');

    // F -> fit screen
    shortcuts.handleKeyDown({ code: 'KeyF', key: 'f', target: { tagName: 'DIV' } });
    assert.strictEqual(triggered, 'fit_screen');

    // Ctrl+Z -> undo
    shortcuts.handleKeyDown({ code: 'KeyZ', key: 'z', ctrlKey: true, target: { tagName: 'DIV' } });
    assert.strictEqual(triggered, 'undo');

    // Input target suppression: typing space inside an input element must not trigger play/pause!
    triggered = null;
    shortcuts.handleKeyDown({ code: 'Space', target: { tagName: 'INPUT' } });
    assert.strictEqual(triggered, null);

    shortcuts.handleKeyDown({ code: 'KeyK', key: 'k', target: { tagName: 'TEXTAREA' } });
    assert.strictEqual(triggered, null);

    shortcuts.handleKeyDown({ code: 'KeyF', key: 'f', target: { isContentEditable: true } });
    assert.strictEqual(triggered, null);
  });

  test('BETA-UI-8.7: Effect model', () => {
    // Opacity with parameter clamping
    const effOp = new Effect({ type: 'Opacity', parameters: { opacity: 1.5 } });
    assert.strictEqual(effOp.parameters.opacity, 1.0);

    const effBright = new Effect({ type: 'Brightness', parameters: { factor: -0.2 } });
    assert.strictEqual(effBright.parameters.factor, 0.0);

    const effTint = new Effect({ type: 'Tint', parameters: { color: '#ff0000', intensity: 0.8 } });
    assert.strictEqual(effTint.parameters.color, '#ff0000');
    assert.strictEqual(effTint.parameters.intensity, 0.8);

    const effFade = new Effect({ type: 'Fade', parameters: { progress: 0.5 } });
    assert.strictEqual(effFade.parameters.progress, 0.5);

    // Toggling enabled
    effFade.enabled = false;
    assert.strictEqual(effFade.enabled, false);

    // Unsupported effect type throws
    assert.throws(() => new Effect({ type: 'QuantumBloom' }), /Unknown effect type/);
  });

  test('BETA-UI-8.8: Effect serialization', () => {
    const node = ComponentRegistry.create('Image', {
      id: 'effect_node',
      x: 0,
      y: 0,
      width: 100,
      height: 100
    });

    node.addEffect({ id: 'eff_1', type: 'Tint', parameters: { color: '#00ff00', intensity: 0.5 } });
    node.addEffect({ id: 'eff_2', type: 'Brightness', parameters: { factor: 1.5 } });
    node.addEffect({ id: 'eff_3', type: 'Fade', parameters: { progress: 0.25 } });

    assert.strictEqual(node.getEffects().length, 3);

    const json = node.toJSON();
    assert.ok(Array.isArray(json.effects));
    assert.strictEqual(json.effects.length, 3);
    assert.strictEqual(json.effects[0].type, 'Tint');
    assert.strictEqual(json.effects[1].type, 'Brightness');
    assert.strictEqual(json.effects[2].type, 'Fade');

    const stack = EffectStack.fromJSON(json.effects);
    assert.strictEqual(stack.getEffects().length, 3);
    assert.strictEqual(stack.getEffects()[0].parameters.color, '#00ff00');

    node.removeEffect('eff_2');
    assert.strictEqual(node.getEffects().length, 2);
    assert.strictEqual(node.getEffect('eff_2'), null);
  });

  test('BETA-UI-8.9: Effect evaluation', () => {
    const scene = new SceneModel({ id: 'EffectAnimScene', durationFrames: 60 });
    const node = ComponentRegistry.create('Shape', {
      id: 'shape_target',
      x: 0,
      y: 0,
      width: 100,
      height: 100
    });
    node.addEffect({
      id: 'eff_tint',
      type: 'Tint',
      parameters: { color: '#ff0000', intensity: 0.0 }
    });
    scene.addNode(node);

    const track = new AnimationTrack({
      targetNodeId: 'shape_target',
      propertyPath: 'effects.0.parameters.intensity'
    });
    track.addKeyframe(0, 0.0, 'linear');
    track.addKeyframe(60, 1.0, 'linear');
    scene.addTrack(track);

    const evalMap0 = TimelineEvaluator.evaluateScene(scene, 0);
    const eval0 = evalMap0.get('shape_target');
    assert.strictEqual(eval0.effects[0].parameters.intensity, 0.0);

    const evalMap30 = TimelineEvaluator.evaluateScene(scene, 30);
    const eval30 = evalMap30.get('shape_target');
    assert.ok(Math.abs(eval30.effects[0].parameters.intensity - 0.5) < 0.01);

    const evalMap60 = TimelineEvaluator.evaluateScene(scene, 60);
    const eval60 = evalMap60.get('shape_target');
    assert.ok(Math.abs(eval60.effects[0].parameters.intensity - 1.0) < 0.01);
  });

  test('BETA-UI-8.10: Blend mode contract', () => {
    assert.strictEqual(BlendModes.isValid('normal'), true);
    assert.strictEqual(BlendModes.isValid('add'), true);
    assert.strictEqual(BlendModes.isValid('multiply'), true);
    assert.strictEqual(BlendModes.isValid('screen'), true);

    // Unsupported blend mode
    assert.strictEqual(BlendModes.isValid('color_burn'), false);
    assert.strictEqual(BlendModes.isValid('soft_light'), false);

    assert.throws(() => BlendModes.assertSupported('vivid_light'), /Unsupported blend mode/);
  });

  test('BETA-UI-8.11: Text node', () => {
    const textNode = new TextNode({
      id: 'txt_header',
      text: 'Title of the Game',
      font: 'standard',
      fontSize: 18,
      align: 'center',
      color: '#facc15',
      lineHeight: 1.2
    });

    assert.strictEqual(textNode.type, 'Text');
    assert.strictEqual(textNode.text, 'Title of the Game');
    assert.strictEqual(textNode.fontSize, 18);
    assert.strictEqual(textNode.align, 'center');
    assert.strictEqual(textNode.color, '#facc15');

    const json = textNode.toJSON();
    assert.strictEqual(json.type, 'Text');
    assert.strictEqual(json.properties.text, 'Title of the Game');
    assert.strictEqual(json.properties.fontSize, 18);

    const bounds = textNode.getBounds();
    assert.ok(bounds.width > 0);
    assert.ok(bounds.height > 0);
  });

  test('BETA-UI-8.12: Text rendering', () => {
    const textNode = new TextNode({
      id: 'txt_render',
      text: 'Sample Text',
      fontSize: 14,
      color: '#ffffff',
      x: 10,
      y: 20,
      width: 100,
      height: 30
    });

    let fontSet = '';
    let fillStyleSet = '';
    let fillTextCalled = false;

    const mockCtx = {
      save: () => {},
      restore: () => {},
      fillText: (str, x, y) => {
        fillTextCalled = true;
        assert.strictEqual(str, 'Sample Text');
      },
      set font(val) { fontSet = val; },
      set fillStyle(val) { fillStyleSet = val; },
      set textAlign(val) {},
      set textBaseline(val) {},
      set globalAlpha(val) {}
    };

    textNode.draw(mockCtx);
    assert.ok(fontSet.includes('14px'));
    assert.strictEqual(fillStyleSet, '#ffffff');
    assert.strictEqual(fillTextCalled, true);
  });

  test('BETA-UI-8.13: Shape node', () => {
    const rect = new ShapeNode({
      id: 'shp_rect',
      shapeType: ShapeTypes.Rectangle,
      fillColor: '#2563eb',
      strokeColor: '#93c5fd',
      strokeWidth: 2
    });
    assert.strictEqual(rect.type, 'Shape');
    assert.strictEqual(rect.shapeType, 'Rectangle');

    const roundRect = new ShapeNode({
      id: 'shp_round',
      shapeType: ShapeTypes.RoundedRectangle,
      cornerRadius: 8
    });
    assert.strictEqual(roundRect.shapeType, 'RoundedRectangle');
    assert.strictEqual(roundRect.cornerRadius, 8);

    const line = new ShapeNode({
      id: 'shp_line',
      shapeType: ShapeTypes.Line,
      strokeWidth: 3
    });
    assert.strictEqual(line.shapeType, 'Line');

    let fillRectCalled = false;
    let strokeCalled = false;

    const mockCtx = {
      save: () => {},
      restore: () => {},
      beginPath: () => {},
      rect: () => { fillRectCalled = true; },
      fillRect: () => { fillRectCalled = true; },
      fill: () => { fillRectCalled = true; },
      stroke: () => { strokeCalled = true; },
      strokeRect: () => { strokeCalled = true; },
      set fillStyle(val) {},
      set strokeStyle(val) {},
      set lineWidth(val) {},
      set globalAlpha(val) {}
    };

    rect.draw(mockCtx);
    assert.strictEqual(fillRectCalled, true);
    assert.strictEqual(strokeCalled, true);
  });

  test('BETA-UI-8.14: Template system', () => {
    const templates = [
      'TitleCard',
      'DialogScene',
      'MenuScene',
      'PokemonEntrance',
      'HUDOverlay',
      'Notification'
    ];

    for (const tName of templates) {
      const tmpl = SceneLibrary.getTemplate(tName);
      assert.ok(tmpl, `Template "${tName}" must exist in SceneLibrary`);
      const scene = SceneLibrary.instantiateTemplate(tName, `Test_${tName}`);
      assert.strictEqual(scene.top.width, 400);
      assert.strictEqual(scene.top.height, 240);
      assert.strictEqual(scene.bottom.width, 320);
      assert.strictEqual(scene.bottom.height, 240);
      assert.ok(scene.nodes.length > 0 || scene.components.length > 0);
    }
  });

  test('BETA-UI-8.15: Nested composition navigation', () => {
    const rootScene = new SceneModel({ id: 'RootScene', name: 'Root Scene' });
    const compA = { id: 'comp_header', name: 'Header Comp', nodes: [] };
    const compB = { id: 'comp_user_card', name: 'User Card Comp', nodes: [] };

    const nav = new CompositionNavigator(rootScene);
    assert.strictEqual(nav.getBreadcrumbsString(), 'Root Scene');
    assert.strictEqual(nav.isAtRoot(), true);

    nav.enterComposition(compA);
    assert.strictEqual(nav.getBreadcrumbsString(), 'Root Scene / Header Comp');
    assert.strictEqual(nav.isAtRoot(), false);

    nav.enterComposition(compB);
    assert.strictEqual(nav.getBreadcrumbsString(), 'Root Scene / Header Comp / User Card Comp');
    assert.strictEqual(nav.getDepth(), 2);

    nav.exitComposition();
    assert.strictEqual(nav.getBreadcrumbsString(), 'Root Scene / Header Comp');

    nav.exitToRoot();
    assert.strictEqual(nav.isAtRoot(), true);
  });

  test('BETA-UI-8.16: Scene tabs', () => {
    const tabs = new SceneTabs();
    tabs.openTab('SceneA', 'Scene A');
    tabs.openTab('SceneB', 'Scene B');
    tabs.openTab('SceneC', 'Scene C');

    assert.strictEqual(tabs.getOpenTabs().length, 3);
    assert.strictEqual(tabs.getActiveTabId(), 'SceneC');

    tabs.setActiveTab('SceneA');
    assert.strictEqual(tabs.getActiveTabId(), 'SceneA');

    tabs.setTabDirty('SceneA', true);
    assert.strictEqual(tabs.isTabDirty('SceneA'), true);

    tabs.closeTab('SceneB');
    assert.strictEqual(tabs.getOpenTabs().length, 2);
  });

  test('BETA-UI-8.17: Dirty state', () => {
    const doc = new ProjectDocument({ name: 'DirtyDoc' });
    assert.strictEqual(doc.isDirty(), false);

    doc.markDirty();
    assert.strictEqual(doc.isDirty(), true);

    doc.markClean();
    assert.strictEqual(doc.isDirty(), false);

    // Adding scene marks project dirty
    doc.addScene({ id: 'NewSceneDirty', name: 'New Scene' });
    assert.strictEqual(doc.isDirty(), true);

    doc.saveProject();
    assert.strictEqual(doc.isDirty(), false);
  });

  test('BETA-UI-8.18: Export profiles', () => {
    const docDebug = new ProjectDocument({ settings: { exportProfile: 'debug' } });
    assert.strictEqual(docDebug.settings.exportProfile, 'debug');

    const docRelease = new ProjectDocument({ settings: { exportProfile: 'release' } });
    assert.strictEqual(docRelease.settings.exportProfile, 'release');

    const docInvalid = new ProjectDocument({ settings: { exportProfile: 'super_turbo' } });
    assert.strictEqual(docInvalid.settings.exportProfile, 'development');
  });

  test('BETA-UI-8.19: Export validation', () => {
    const validProj = new ProjectDocument({ name: 'ValidProj' });
    assert.doesNotThrow(() => ProjectValidator.assertCanExport(validProj));

    const invalidProj = new ProjectDocument({
      name: 'InvalidProj',
      scenes: [
        new SceneModel({
          id: 'BadScene'
        })
      ]
    });
    // ProjectDocument repairs invalid selections during construction; model a
    // corrupted persisted document after construction so export validation is tested.
    invalidProj.activeSceneId = 'non_existent_scene';

    assert.throws(() => ProjectValidator.assertCanExport(invalidProj), /EXPORT BLOCKED/);
  });

  test('BETA-UI-8.20: Frame capture', () => {
    const renderer = {
      canvasTop: {
        toDataURL: (fmt) => 'data:image/png;base64,MOCK_FRAME_TOP'
      },
      canvasBottom: {
        toDataURL: (fmt) => 'data:image/png;base64,MOCK_FRAME_BOTTOM'
      }
    };

    const topCapture = CanvasRenderer.exportFrame(renderer, 'top');
    assert.strictEqual(topCapture, 'data:image/png;base64,MOCK_FRAME_TOP');

    const bottomCapture = CanvasRenderer.exportFrame(renderer, 'bottom');
    assert.strictEqual(bottomCapture, 'data:image/png;base64,MOCK_FRAME_BOTTOM');
  });

  test('BETA-UI-8.21: Scene thumbnails', () => {
    const scene = new SceneModel({ id: 'ThumbScene', name: 'Thumb Scene' });
    const thumbA = CanvasRenderer.generateThumbnail(scene);
    const thumbB = CanvasRenderer.generateThumbnail(scene);

    assert.strictEqual(thumbA, thumbB, 'Thumbnails must be 100% deterministic');
    assert.ok(typeof thumbA === 'string');
    assert.ok(!thumbA.includes('Date'));
  });

  test('BETA-UI-8.22: Export report', () => {
    const scene = new SceneModel({ id: 'ReportScene', durationFrames: 90, fps: 60 });
    const report = ExportReport.generateReport(scene, {
      romfsBytes: 153332,
      elfSize: 2211808,
      _3dsxSize: 153332
    });

    assert.strictEqual(report.scene, 'ReportScene');
    assert.strictEqual(report.fps, 60);
    assert.strictEqual(report.duration, 90);
    assert.strictEqual(report.romfsBytes, 153332);
    assert.strictEqual(report.elfSize, 2211808);

    const text = ExportReport.formatReportText(report);
    assert.ok(text.includes('EXPORT REPORT'));
    assert.ok(text.includes('ReportScene'));
    assert.ok(text.includes('2211808'));
  });

  test('BETA-UI-8.23: Asset references', () => {
    const project = new ProjectDocument({
      name: 'AssetRefProj',
      scenes: [
        new SceneModel({
          id: 'SceneA',
          nodes: [
            { id: 'img_1', type: 'Image', x: 0, y: 0, width: 10, height: 10, properties: { asset: 'bg_arena_plains' } }
          ]
        }),
        new SceneModel({
          id: 'SceneB',
          nodes: [
            { id: 'img_2', type: 'Image', x: 0, y: 0, width: 10, height: 10, properties: { asset: 'bg_arena_plains' } },
            { id: 'img_3', type: 'Image', x: 0, y: 0, width: 10, height: 10, properties: { asset: 'other_asset' } }
          ]
        })
      ]
    });

    const refs = DependencyGraph.findAssetReferences(project, 'bg_arena_plains');
    assert.strictEqual(refs.length, 2);
    assert.ok(refs.some(r => r.sceneId === 'SceneA' && r.nodeId === 'img_1'));
    assert.ok(refs.some(r => r.sceneId === 'SceneB' && r.nodeId === 'img_2'));
  });

  test('BETA-UI-8.24: Scene references', () => {
    const project = new ProjectDocument({
      name: 'SceneRefProj',
      scenes: [
        new SceneModel({
          id: 'MainScene',
          nodes: [
            { id: 'comp_node', type: 'Composition', x: 0, y: 0, width: 10, height: 10, properties: { compositionId: 'SubComp' } }
          ]
        })
      ]
    });

    const refs = DependencyGraph.findSceneReferences(project, 'SubComp');
    assert.strictEqual(refs.length, 1);
    assert.strictEqual(refs[0].sceneId, 'MainScene');
    assert.strictEqual(refs[0].nodeId, 'comp_node');
  });

  test('BETA-UI-8.25: Safe delete', () => {
    const project = new ProjectDocument({
      name: 'SafeDelProj',
      scenes: [
        new SceneModel({
          id: 'Scene1',
          nodes: [{ id: 'n1', type: 'Image', properties: { asset: 'protected_asset' } }]
        })
      ]
    });

    const checkBlocked = DependencyGraph.safeDelete(project, 'asset', 'protected_asset');
    assert.strictEqual(checkBlocked.canDelete, false);
    assert.ok(checkBlocked.references.length > 0);

    const checkAllowed = DependencyGraph.safeDelete(project, 'asset', 'unreferenced_asset');
    assert.strictEqual(checkAllowed.canDelete, true);
    assert.strictEqual(checkAllowed.references.length, 0);
  });

  test('BETA-UI-8.26: Batch export', () => {
    const scene1 = new SceneModel({ id: 'BatchScene1', name: 'Batch 1' });
    const scene2 = new SceneModel({ id: 'BatchScene2', name: 'Batch 2' });

    const results = BatchExporter.exportScenes([scene1, scene2]);
    assert.strictEqual(results.length, 2);
    assert.strictEqual(results[0].sceneId, 'BatchScene1');
    assert.strictEqual(results[1].sceneId, 'BatchScene2');
    assert.ok(results[0].cppExport.dataHpp.includes('g_SceneDefinition'));
    assert.ok(results[1].cppExport.dataHpp.includes('g_SceneDefinition'));
  });

  test('BETA-UI-8.27: Batch build', () => {
    const project = new ProjectDocument({
      name: 'BatchBuildProj',
      scenes: [
        new SceneModel({ id: 'BuildA', name: 'Build Scene A' }),
        new SceneModel({ id: 'BuildB', name: 'Build Scene B' })
      ]
    });

    const batchRes = BatchExporter.buildAll(project);
    assert.strictEqual(batchRes.success, true);
    assert.strictEqual(batchRes.exports.length, 2);
    assert.strictEqual(batchRes.errors.length, 0);
  });

  test('BETA-UI-8.28: Demo project', () => {
    const demoPath = path.join(__dirname, '../project/demo/DemoProject.json');
    assert.ok(fs.existsSync(demoPath), 'project/demo/DemoProject.json must exist');

    const demoDoc = ProjectDocument.loadProject(fs.readFileSync(demoPath, 'utf8'));
    assert.strictEqual(demoDoc.name, 'DemoProject');
    assert.strictEqual(demoDoc.getScenes().length, 7);

    const sceneIds = demoDoc.getScenes().map(s => s.id);
    assert.ok(sceneIds.includes('Title'));
    assert.ok(sceneIds.includes('Menu'));
    assert.ok(sceneIds.includes('Dialog'));
    assert.ok(sceneIds.includes('PokemonEntrance'));
    assert.ok(sceneIds.includes('DualScreen'));
    assert.ok(sceneIds.includes('ProfessionalComposition'));
    assert.ok(sceneIds.includes('AdvancedAnimation'));

    const val = ProjectValidator.validate(demoDoc);
    assert.strictEqual(val.valid, true, 'Demo project must pass validation with 0 errors');
  });

  test('BETA-UI-8.29: Runtime metrics', () => {
    const headerPath = path.join(__dirname, '../project/include/runtime/RuntimeAssetManager.hpp');
    const header = fs.readFileSync(headerPath, 'utf8');

    assert.ok(header.includes('uint32_t effectEvaluations;'), 'Must track effectEvaluations metric');
    assert.ok(header.includes('uint32_t textNodes;'), 'Must track textNodes metric');
    assert.ok(header.includes('uint32_t shapeNodes;'), 'Must track shapeNodes metric');
    assert.ok(header.includes('uint32_t nestedCompositionDepth;'), 'Must track nestedCompositionDepth metric');
  });

  test('BETA-UI-8.30: Memory instrumentation', () => {
    const headerPath = path.join(__dirname, '../project/include/runtime/RuntimeAssetManager.hpp');
    const header = fs.readFileSync(headerPath, 'utf8');

    assert.ok(header.includes('uint32_t assetMemoryBytes;'), 'Must track assetMemoryBytes');
    assert.ok(header.includes('uint32_t sceneMemoryBytes;'), 'Must track sceneMemoryBytes');
    assert.ok(header.includes('uint32_t runtimeCacheBytes;'), 'Must track runtimeCacheBytes');
    assert.ok(header.includes('uint32_t effectMemoryBytes;'), 'Must track effectMemoryBytes');
  });

  test('BETA-UI-8.31: Schema migration', () => {
    const v4Data = {
      schemaVersion: 4,
      id: 'LegacyV4',
      name: 'Legacy V4 Scene',
      nodes: [
        { id: 'img_node', type: 'Image', x: 10, y: 10, width: 50, height: 50 }
      ],
      compositions: [],
      guides: [],
      safeAreas: []
    };

    const v5Data = SceneModel.migrateV4ToV5(v4Data);
    assert.strictEqual(v5Data.schemaVersion, 5);
    assert.strictEqual(v5Data.nodes.length, 1);
    assert.ok(Array.isArray(v5Data.nodes[0].effects));
  });

  test('BETA-UI-8.32: Backward compatibility', () => {
    const v1Data = { schemaVersion: 1, id: 'V1', components: [{ id: 'c1', type: 'Image' }] };
    const v2Data = { schemaVersion: 2, id: 'V2', top: { width: 400 }, components: [{ id: 'c2', type: 'Image' }] };
    const v3Data = { schemaVersion: 3, id: 'V3', tracks: [{ id: 't1', nodeId: 'c3', property: 'x' }], components: [] };
    const v4Data = { schemaVersion: 4, id: 'V4', compositions: [{ id: 'comp1', nodes: [] }], nodes: [] };

    assert.doesNotThrow(() => new SceneModel(v1Data));
    assert.doesNotThrow(() => new SceneModel(v2Data));
    assert.doesNotThrow(() => new SceneModel(v3Data));
    assert.doesNotThrow(() => new SceneModel(v4Data));
  });

  test('BETA-UI-8.33: Determinism', () => {
    const project = new ProjectDocument({ name: 'DetProject' });
    const jsonA = JSON.stringify(project.saveProject());
    const jsonB = JSON.stringify(project.saveProject());
    assert.strictEqual(jsonA, jsonB);

    const scene = new SceneModel({ id: 'DetScene', durationFrames: 60 });
    const expA = SceneCppExporter.export(scene);
    const expB = SceneCppExporter.export(scene);
    assert.strictEqual(expA.dataHpp, expB.dataHpp);
    assert.strictEqual(expA.dataCpp, expB.dataCpp);
    assert.strictEqual(expA.timelineHpp, expB.timelineHpp);
    assert.strictEqual(expA.timelineCpp, expB.timelineCpp);
  });

  test('BETA-UI-8.34: Preview/runtime parity', () => {
    const scene = new SceneModel({ id: 'ParityScene', durationFrames: 60 });
    const shape = ComponentRegistry.create('Shape', {
      id: 'shape_box',
      x: 10,
      y: 20,
      width: 50,
      height: 50,
      properties: { shapeType: 'RoundedRectangle', cornerRadius: 4 }
    });
    scene.addNode(shape);

    const track = new AnimationTrack({ targetNodeId: 'shape_box', propertyPath: 'transform.x' });
    track.addKeyframe(0, 10, 'linear');
    track.addKeyframe(60, 100, 'linear');
    scene.addTrack(track);

    const exportModel = SceneCppExporter.export(scene).exportModel;
    const jsEvaluations = TimelineEvaluator.evaluateScene(scene, 30);
    const cppEvaluations = SceneCppExporter.evaluateExportedData(exportModel, 30);

    const jsNode = jsEvaluations.get('shape_box');
    const jsX = jsNode.transform.x !== undefined ? jsNode.transform.x : shape.x;
    const jsY = jsNode.transform.y !== undefined ? jsNode.transform.y : shape.y;

    const cppNode = cppEvaluations.get('shape_box');
    assert.ok(cppNode);
    assert.ok(Math.abs(jsX - cppNode.transform.x) < 0.001);
    assert.ok(Math.abs(jsY - cppNode.transform.y) < 0.001);
  });

  test('BETA-UI-8.35: Export/runtime parity', () => {
    const scene = new SceneModel({ id: 'ExportCheckScene' });
    const textNode = ComponentRegistry.create('Text', {
      id: 'text_node',
      x: 0,
      y: 0,
      width: 100,
      height: 20,
      properties: { text: 'Parity Text' }
    });
    scene.addNode(textNode);

    const exported = SceneCppExporter.export(scene);
    assert.ok(exported.dataHpp.includes('struct SceneNodeData'));
    assert.ok(exported.dataHpp.includes('SceneTextData textData;'));
    assert.ok(exported.dataCpp.includes('"Parity Text"'));
  });

  test('BETA-UI-8.36: Real 3DS build regression', () => {
    // 1. Strict gameplay guardrail: BattleEngine, BattleSession, BattleState remain intact
    assert.ok(BattleEngine, 'BattleEngine must exist');
    assert.ok(BattleSession, 'BattleSession must exist');
    assert.ok(BattleState, 'BattleState must exist');
    assert.strictEqual(typeof BattleEngine.prototype.executeCommand, 'function', 'BattleEngine.executeCommand remains unchanged');

    // 2. Real devkitARM binaries verify
    const elfPath = path.join(__dirname, '../build/GUI_3DS.elf');
    const _3dsxPath = path.join(__dirname, '../build/GUI_3DS.3dsx');
    assert.ok(fs.existsSync(elfPath), 'build/GUI_3DS.elf must exist');
    assert.ok(fs.existsSync(_3dsxPath), 'build/GUI_3DS.3dsx must exist');
    assert.ok(fs.statSync(elfPath).size > 1000000, 'ELF must be real binary > 1MB');
    assert.ok(fs.statSync(_3dsxPath).size > 50000, '3DSX must be real binary > 50KB');
  });

  test('BETA-UI-8C: pinned real canonical content migration', async () => {
    const repository = new PokerogueRepository();
    const importer = new PokerogueImporter(repository);
    const result = await importer.importCanonicalContent(repository);
    assert.strictEqual(result.sourceType, 'UPSTREAM');
    assert.strictEqual(result.canonicalContent.sourceSnapshot.revision, '8555c08c823b856cbec4eb99ca84ea52a955836d');
    assert.ok(result.species.length > 1000);
    assert.ok(result.forms.length > 0);
    assert.ok(result.moves.some(move => move.type === 'ELECTRIC') && result.moves.some(move => move.type === 'FIRE'));
    assert.ok(result.moves.some(move => move.category === 'Physical') && result.moves.some(move => move.category === 'Special'));
    assert.ok(result.abilities.some(ability => ability.attributes.some(attribute => attribute.kind === 'UPSTREAM_DECLARATION')));
    assert.ok(result.items.length > 0 && result.items.every(item => item.source.sourceType === 'UPSTREAM' && item.price === null));
    assert.ok(result.gameModes.length > 0);
    assert.ok(result.locales.some(entry => entry.namespace === 'move' && entry.canonicalId === 'tackle'));
    assert.strictEqual(importer.localeImporter.resolveCanonical(result.locales, 'tackle', 'en', 'move'), 'Tackle');
    assert.strictEqual(result.canonicalContent.collections.assetReferences.filter(asset => asset.verified).length, 3);
    assert.ok(result.species.some(species => species.id === 'bulbasaur' && species.speciesId === 1));
    assert.ok(result.species.some(species => species.id === 'pikachu' && species.speciesId === 25));
    const bulbasaur = result.species.find(species => species.id === 'bulbasaur');
    assert.equal(bulbasaur.evolutions[0].targetSpeciesId, 'ivysaur');
    assert.equal(bulbasaur.evolutions[0].level, 16);
    assert.equal(bulbasaur.evolutions[0].source.sourcePath, 'src/data/balance/species/generation-01.ts');
    const golbat = result.species.find(species => species.id === 'golbat');
    assert.ok(golbat.evolutions.some(edge => edge.targetSpeciesId === 'crobat' && edge.evoLevelThreshold.wild === 54));
    assert.ok(result.forms.every(form => result.species.some(species => species.id === form.speciesId)));
    assert.ok(result.abilities.every(ability => ability.extensions.runtimeBehavior === 'NOT_IMPORTED'));
    assert.ok(result.canonicalContent.hash() === result.importReport.contentHash);
    assert.strictEqual(result.canonicalContent.sourceSnapshot.sources.length, result.importReport.provenance.length);
    const manager = new DataManager();
    await manager.importCanonicalProduction({ repository, importCanonicalContent: async () => result }, repository);
    assert.strictEqual(manager.isFallback, false);
    assert.strictEqual(manager.getCanonicalCollection('species').length, result.species.length, 'production canonical collection contains the complete upstream catalog');
    assert.strictEqual(manager.getCanonicalCollection('species').find(species => species.id === 'pikachu').speciesId, 25);
    assert.strictEqual(manager.runtimeBridgeIsFixture, true, 'prototype BattleEngine bridge remains outside the canonical production path');
    assert.strictEqual(manager.getLocalizedText('move', 'tackle'), 'Tackle');
    assert.ok(manager.getCanonicalEnum('SpeciesId'));
    await assert.rejects(() => manager.importCanonicalProduction({ importCanonicalContent: async () => ({ ...result, sourceType: 'TEST_FIXTURE' }) }, repository), /must be pinned UPSTREAM/);
  });

  registerBetaUI9ATests(test);
  registerBetaUI9CRngTests(test);
  registerBetaUI9DTests(test);

}
