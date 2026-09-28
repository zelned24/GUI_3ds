import assert from 'assert';
import { readFile } from 'node:fs/promises';
import { PokerogueImporter } from '../public/js/data/PokerogueImporter.js';
import { PokerogueRepository } from '../public/js/data/PokerogueRepository.js';
import { FirstRunFlow } from '../public/js/game/FirstRunFlow.js';
import { CanonicalContent, RuntimeContent } from '../public/js/data/CanonicalDataContract.js';
import { DataManager } from '../public/js/data/DataManager.js';

export function registerBetaUI9ATests(test) {
  test('BETA-UI-9A: pinned upstream content completes a deterministic first-run presentation flow', async () => {
    const importer = new PokerogueImporter(new PokerogueRepository());
    const imported = await importer.importPlayableCanonicalContent(undefined, { generations: [1] });
    const repeatedImport = await importer.importPlayableCanonicalContent(undefined, { generations: [1] });
    assert.strictEqual(repeatedImport.importReport.contentHash, imported.importReport.contentHash, 'same pins/import/normalization produce the same canonical hash');
    assert.strictEqual(imported.canonicalContent.extensions.biomePoolReferenceAudit.status, 'PARTIAL_SPECIES_SNAPSHOT_UNVERIFIED', 'filtered generation imports declare cross-reference limits');
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

  test('BETA-UI-9A: production first-run pipeline rejects fixture content', () => {
    const fixture = new CanonicalContent({ sourceSnapshot: { repository: 'test', revision: 'fixture', sourceType: 'TEST_FIXTURE' }, provenance: { sourceRepository: 'test', sourceRevision: 'fixture', sourcePath: 'test/fixtures/content.json', sourceType: 'TEST_FIXTURE' }, collections: {} });
    assert.throws(() => new FirstRunFlow({ runtimeContent: new RuntimeContent({ canonicalContent: fixture }) }), /fixture\/non-UPSTREAM/);
  });

  test('BETA-UI-9A: Old 3DS C++ bundle matches pinned import report and carries real catalogs', async () => {
    const [header, reportText] = await Promise.all([
      readFile('project/generated/include/content/PokerogueRuntimeContent.hpp', 'utf8'),
      readFile('project/data/pokerogue/import-report.json', 'utf8'),
    ]);
    const report = JSON.parse(reportText);
    assert.ok(header.includes(`kContentHash[] = "${report.contentHash}"`));
    assert.ok(header.includes(`kPokerogueRevision[] = "${report.sourceRevisions.pokerogue}"`));
    for (const domain of ['kSpecies', 'kForms', 'kMoves', 'kAbilities', 'kItems', 'kLocales', 'kModes', 'kBiomes', 'kBiomeEncounterPools', 'kBiomeTrainerPools', 'kRoutes']) {
      assert.ok(header.includes(`${domain}[] = {`), `native ROM bundle contains ${domain}`);
    }
    assert.match(header, /\{1, 1, 3, true, "bulbasaur", "Bulbasaur"/);
    assert.ok(header.includes('"en:pokemon:bulbasaur", "Bulbasaur"'), 'native runtime resolves names from imported locale records');
    assert.ok(header.includes('kStartingBiomeId[] = "plains"'));
    assert.ok(header.includes('{"plains", "common", "dawn", "sentret"'), 'native encounter pool rows preserve real Plains species membership');
  });
}
