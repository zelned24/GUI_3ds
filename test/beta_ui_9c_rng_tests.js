import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const rootDir = path.resolve(testDir, '..');

function resolveClang() {
  const candidates = process.platform === 'win32'
    ? [path.join(rootDir, 'node_modules', 'clang-wasm-win64', 'clang.exe')]
    : [
        path.join(rootDir, 'node_modules', 'clang-linux-x64', 'bin', 'clang'),
        path.join(rootDir, 'node_modules', 'clang-wasm-linux-x64', 'clang'),
        path.join(rootDir, 'node_modules', '.bin', 'clang'),
      ];
  return candidates.find(candidate => fs.existsSync(candidate)) || null;
}

function phaserReference(seed) {
  let n = 0xefc8249d;
  const hash = value => {
    for (let i = 0; i < value.length; i++) {
      n += value.charCodeAt(i);
      let h = 0.02519603282416938 * n;
      n = h >>> 0; h -= n; h *= n;
      n = h >>> 0; h -= n; n += h * 0x100000000;
    }
    return (n >>> 0) * 2.3283064365386963e-10;
  };
  let c = 1;
  let s0 = hash(' '), s1 = hash(' '), s2 = hash(' ');
  s0 -= hash(seed); if (s0 < 0) s0++;
  s1 -= hash(seed); if (s1 < 0) s1++;
  s2 -= hash(seed); if (s2 < 0) s2++;
  const rnd = () => {
    const t = 2091639 * s0 + c * 2.3283064365386963e-10;
    c = t | 0; s0 = s1; s1 = s2; s2 = t - c;
    return s2;
  };
  const frac = () => rnd() + ((rnd() * 0x200000) | 0) * 1.1102230246251565e-16;
  const int = range => Math.floor(frac() * range);
  return { int, frac, state: () => ({ c, s0, s1, s2 }) };
}

function shiftedSeed(seed, offset) {
  let value = '';
  for (let i = 0; i < seed.length; i++) value += String.fromCharCode((seed.charCodeAt(i) + offset) & 0xffff);
  return value;
}

function resolveWildSpeciesReference(canonical, speciesId, level, rng, allowEvolving = true, forcePrevo = true) {
  const species = canonical.collections.species;
  const evolutionEdges = species.flatMap(record => (record.evolutions || []).map(edge => ({ sourceSpeciesId: record.id, ...edge })));
  if (forcePrevo) {
    const prevolutions = [];
    const collect = (targetId, depth = 0) => {
      if (depth >= 32) return;
      for (const edge of evolutionEdges.filter(candidate => candidate.targetSpeciesId === targetId)) {
        if (prevolutions.some(existing => existing.sourceSpeciesId === edge.sourceSpeciesId)) continue;
        const threshold = edge.evoLevelThreshold?.wild ?? edge.level;
        const required = edge.level === 1 ? threshold : Math.min(edge.level, threshold);
        prevolutions.push({ sourceSpeciesId: edge.sourceSpeciesId, required });
        collect(edge.sourceSpeciesId, depth + 1);
      }
    };
    collect(speciesId);
    for (let i = prevolutions.length - 1; i >= 0; i--) {
      if (level < prevolutions[i].required) return prevolutions[i].sourceSpeciesId;
    }
  }
  if (!allowEvolving) return speciesId;
  const eligible = evolutionEdges.filter(edge => edge.sourceSpeciesId === speciesId).map(edge => ({
    edge,
    threshold: Math.max(edge.level, edge.evoLevelThreshold?.wild ?? 0),
  })).filter(item => level >= item.edge.level && level >= item.threshold);
  if (!eligible.length) return speciesId;
  const selected = eligible.length === 1 ? eligible[0] : eligible[rng.int(eligible.length)];
  const randomMax = Math.floor(selected.threshold * 1.2 + 0.5);
  const randomLevel = randomMax <= selected.threshold
    ? selected.threshold
    : selected.threshold + rng.int(randomMax - selected.threshold + 1);
  return randomLevel <= level
    ? resolveWildSpeciesReference(canonical, selected.edge.targetSpeciesId, level, rng, true, false)
    : speciesId;
}

export function registerBetaUI9CRngTests(test) {
  test('BETA-UI-9C: Old 3DS Phaser RNG adapter matches pinned Phaser 3.90.0 golden vectors', async () => {
    const clang = resolveClang();
    if (!clang) {
      const error = new Error('BLOCKED — missing toolchain/dependency: clang-wasm; install from package-lock.json with npm ci');
      error.isToolchainBlocked = true;
      throw error;
    }
    const buildDir = path.join(testDir, 'native', 'build');
    fs.mkdirSync(buildDir, { recursive: true });
    const wasmPath = path.join(buildDir, 'pokerogue_rng_harness.wasm');
    execFileSync(clang, [
      '--target=wasm32', '-O2', '-nostdlib', '-fno-rtti', '-fno-exceptions',
      '-Wl,--no-entry', '-Wl,--export-all',
      `-I${path.join(rootDir, 'project', 'include')}`,
      `-I${path.join(rootDir, 'project', 'generated', 'include')}`,
      `-I${path.join(testDir, 'native', 'host_compat')}`,
      '-o', wasmPath,
      path.join(testDir, 'native', 'pokerogue_rng_harness.cpp'),
    ], { stdio: 'pipe' });
    const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath));
    const api = instance.exports;
    const close = (actual, expected, label) => assert.ok(Math.abs(actual - expected) <= 1e-15, `${label}: ${actual} != ${expected}`);
    const readString = pointer => {
      const bytes = new Uint8Array(api.memory.buffer);
      let end = pointer; while (bytes[end]) end++;
      return new TextDecoder().decode(bytes.subarray(pointer, end));
    };

    // Golden values were generated from Phaser v3.90.0 RandomDataGenerator.js
    // sow/rnd/frac/integerInRange/pick semantics, using seeds that exercise
    // root, shifted wave and standalone executeWithSeedOffset derivation.
    close(api.harness_pokerogue_rng_fraction(), 0.743767629869303, 'first Phaser frac()');
    assert.equal(api.harness_pokerogue_rng_range_511(), 380, 'root seed range [0, 511]');
    assert.equal(api.harness_pokerogue_rng_wave1_range_511(), 42, 'wave seed shiftCharCodes(seed, 1)');
    assert.equal(api.harness_pokerogue_rng_offset4_range_99(), 92, 'executeWithSeedOffset(seed, 4)');
    assert.equal(api.harness_pokerogue_rng_seed_offset0_range_7(), 5, 'seed offset zero');
    assert.equal(api.harness_pokerogue_rng_empty_pick_is_rejected(), 1, 'empty pool is explicitly rejected without fallback');
    assert.equal(api.harness_pokerogue_rng_singleton_is_no_draw(), 1, 'singleton randSeedItem consumes no RNG');
    assert.equal(api.harness_pokerogue_rng_range_one_is_no_draw(), 1, 'randSeedInt(range <= 1) consumes no RNG');
    assert.equal(api.harness_pokerogue_rng_state_roundtrip(), 1, 'serializable Phaser RNG state restores the next draw');
    assert.equal(api.harness_pokerogue_rng_offset_restores_state(), 1, 'seed-offset scope restores caller RNG state');
    assert.equal(api.harness_pokerogue_shift_wraps_utf16(), 1, 'shiftCharCodes follows UTF-16 modulo semantics');
    assert.equal(api.harness_pokerogue_wave_cycle_offset(), 1, 'Classic wave cycle uses root randSeedInt(8) multiplied by five');
    assert.equal(api.harness_pokerogue_time_of_day_boundaries(), 1, 'Arena.getTimeOfDay boundaries and 40-wave wrap match upstream');

    // Independent JS reference follows pinned Phaser draws and the Arena pool
    // source contract; C++ resolves the checked-in real generated Town pool.
    const rootSeed = 'pokerogue-rng-v1';
    close(phaserReference(rootSeed).frac(), 0.743767629869303, 'independent Phaser reference first fraction');
    assert.equal(phaserReference(shiftedSeed(rootSeed, 1)).int(512), 42, 'independent Phaser wave-seed golden');
    const canonical = JSON.parse(fs.readFileSync(path.join(rootDir, 'project/data/pokerogue/canonical-content.json'), 'utf8'));
    const rootRng = phaserReference(rootSeed);
    const cycleOffset = rootRng.int(8) * 5;
    const cycle = (1 + cycleOffset) % 40;
    const expectedTime = cycle < 15 ? 'day' : cycle < 20 ? 'dusk' : cycle < 35 ? 'night' : 'dawn';
    const waveRng = phaserReference(shiftedSeed(rootSeed, 1));
    close(api.harness_wave1_first_fraction(), phaserReference(shiftedSeed(rootSeed, 1)).frac(), 'first shifted-wave fraction');
    const doubleRoll = waveRng.int(8);
    const afterDouble = phaserReference(shiftedSeed(rootSeed, 1)); afterDouble.int(8);
    const tierRoll = waveRng.int(512);
    const nextMemberFraction = waveRng.frac();
    const tier = tierRoll >= 156 ? 'common' : tierRoll >= 32 ? 'uncommon' : tierRoll >= 6 ? 'rare' : tierRoll >= 1 ? 'super_rare' : 'ultra_rare';
    const town = canonical.collections.biomes.find(entry => entry.id === 'town');
    const tiers = ['common', 'uncommon', 'rare', 'super_rare', 'ultra_rare'];
    let selectedTier = tier;
    let pool = [];
    while (tiers.includes(selectedTier)) {
      pool = [...(town.encounterPools[selectedTier]?.all || []), ...(town.encounterPools[selectedTier]?.[expectedTime] || [])];
      if (pool.length || selectedTier === 'common') break;
      selectedTier = tiers[tiers.indexOf(selectedTier) - 1];
    }
    const memberRng = phaserReference(shiftedSeed(rootSeed, 1));
    memberRng.int(8); memberRng.int(512);
    const memberIndex = pool.length > 1 ? memberRng.int(pool.length) : 0;
    const levelRng = phaserReference(shiftedSeed(rootSeed, 8));
    for (let i = 0; i < 16; i++) levelRng.int(62);
    let levelRandomSum = 0;
    for (let i = 0; i < 10; i++) levelRandomSum += levelRng.frac();
    const expectedLevel = Math.max(Math.round(1 + 1 / 2 + (1 / 25) ** 2 + Math.abs(levelRandomSum / 10)), 1);
    const fractionalDeviationRng = phaserReference(shiftedSeed(rootSeed, 88));
    for (let i = 0; i < 16; i++) fractionalDeviationRng.int(62);
    const fractionalDeviation = 10 / 11;
    let fractionalDeviationSum = 0;
    for (let i = fractionalDeviation; i > 0; i--) fractionalDeviationSum += fractionalDeviationRng.frac();
    const expectedWave11Level = Math.max(Math.round(1 + 11 / 2 + (11 / 25) ** 2 + Math.abs(fractionalDeviationSum / fractionalDeviation)), 1);
    const memory = new Uint8Array(api.memory.buffer);
    const ptr = api.harness_wave1_species_id();
    let end = ptr; while (memory[end]) end++;
    const actualSpecies = new TextDecoder().decode(memory.subarray(ptr, end));
    assert.equal(api.harness_wave1_cycle_offset(), cycleOffset, 'wave cycle offset matches independent Phaser reference');
    assert.equal(api.harness_wave1_time_of_day(), ['day', 'dusk', 'night', 'dawn'].indexOf(expectedTime), 'wave-1 time-of-day matches upstream clock');
    assert.equal(api.harness_wave1_double_roll(), doubleRoll, 'Classic wave-1 double check consumes first wave stream draw');
    assert.equal(api.harness_wave1_double_battle(), doubleRoll === 0 ? 1 : 0);
    close(api.harness_wave1_state_s0_after_double(), afterDouble.state().s0, 'state s0 after double draw');
    close(api.harness_wave1_state_s1_after_double(), afterDouble.state().s1, 'state s1 after double draw');
    close(api.harness_wave1_state_s2_after_double(), afterDouble.state().s2, 'state s2 after double draw');
    close(api.harness_wave1_state_c_after_double(), afterDouble.state().c, 'state carry after double draw');
    assert.equal(api.harness_wave1_tier_roll(), tierRoll, 'Arena tier consumes the next wave stream draw');
    const expectedState = phaserReference(shiftedSeed(rootSeed, 1));
    expectedState.int(8); expectedState.int(512);
    close(api.harness_wave1_state_s0_after_tier(), expectedState.state().s0, 'state s0 after tier draw');
    close(api.harness_wave1_state_s1_after_tier(), expectedState.state().s1, 'state s1 after tier draw');
    close(api.harness_wave1_state_s2_after_tier(), expectedState.state().s2, 'state s2 after tier draw');
    close(api.harness_wave1_state_c_after_tier(), expectedState.state().c, 'state carry after tier draw');
    close(api.harness_wave1_next_fraction_after_tier(), nextMemberFraction, 'member draw starts at next matching Phaser fraction');
    assert.equal(api.harness_wave1_pool_size(), pool.length, 'candidate pool is real Town ALL entries followed by time entries');
    assert.equal(api.harness_wave1_member_index(), memberIndex, 'Arena member choice consumes the next draw if needed');
    assert.equal(api.harness_wave1_legend_rerolls(), 0, 'Town wave-1 pool candidate needs no LegendLike/BST reroll');
    assert.equal(actualSpecies, pool[memberIndex], 'native candidate matches pinned source pool order and Phaser draws');
    const encounterRng = phaserReference(shiftedSeed(rootSeed, 1));
    encounterRng.int(8); encounterRng.int(512);
    if (pool.length > 1) encounterRng.int(pool.length);
    const expectedResolvedSpecies = resolveWildSpeciesReference(canonical, actualSpecies, expectedLevel, encounterRng);
    assert.equal(readString(api.harness_wave1_resolved_species_id(expectedLevel)), expectedResolvedSpecies,
      'Arena.randomSpecies applies the pinned getWildSpeciesForLevel substitution after pool selection');
    assert.equal(api.harness_wave1_non_boss_level(), expectedLevel, 'Battle.getLevelForWave uses the battle-scoped seed and randSeedGaussForLevel draw count');
    assert.equal(api.harness_wave11_non_boss_level(), expectedWave11Level, 'fractional randSeedGaussForLevel deviation consumes the pinned loop draw count');

    assert.equal(readString(api.harness_forced_prevolution_species()), 'ivysaur',
      'getRequiredPrevo forces Ivysaur for a level-17 Venusaur without an RNG draw');
    const evolutionRng = phaserReference('evo-1');
    const bulbasaur = canonical.collections.species.find(species => species.id === 'bulbasaur');
    const eligibleEvolutions = bulbasaur.evolutions.filter(edge => {
      const required = Math.max(edge.level, edge.evoLevelThreshold?.wild ?? 0);
      return 18 >= edge.level && 18 >= required;
    });
    const chosenEvolution = eligibleEvolutions[evolutionRng.int(eligibleEvolutions.length)];
    const evolutionThreshold = Math.max(chosenEvolution.level, chosenEvolution.evoLevelThreshold?.wild ?? 0);
    const evolutionMax = Math.floor(evolutionThreshold * 1.2 + 0.5);
    const randomEvolutionLevel = evolutionThreshold + evolutionRng.int(evolutionMax - evolutionThreshold + 1);
    const expectedEvolution = randomEvolutionLevel <= 18 ? chosenEvolution.targetSpeciesId : 'bulbasaur';
    assert.equal(readString(api.harness_level_evolution_species()), expectedEvolution,
      'wild-level evolution selection follows pinned randSeedItem and randSeedIntRange decisions');

    // A second pinned real biome/seed deliberately selects Latios first;
    // Arena.checkLegendBST must consume a fresh tier/member draw and reroll.
    const rerollSeed = 'reroll-146';
    const rerollRoot = phaserReference(rerollSeed);
    const rerollOffset = rerollRoot.int(8) * 5;
    const rerollCycle = (1 + rerollOffset) % 40;
    const rerollTime = rerollCycle < 15 ? 'day' : rerollCycle < 20 ? 'dusk' : rerollCycle < 35 ? 'night' : 'dawn';
    const rerollRng = phaserReference(shiftedSeed(rerollSeed, 1));
    rerollRng.int(8);
    const plains = canonical.collections.biomes.find(entry => entry.id === 'plains');
    const speciesById = new Map(canonical.collections.species.map(species => [species.id, species]));
    let expectedRerolls = 0;
    let expectedFinalSpecies = null;
    for (let attempt = 0; attempt <= 10; attempt++) {
      const roll = rerollRng.int(512);
      let index = roll >= 156 ? 0 : roll >= 32 ? 1 : roll >= 6 ? 2 : roll >= 1 ? 3 : 4;
      let rerollPool;
      do {
        const tierPools = plains.encounterPools[tiers[index]] || {};
        rerollPool = [...(tierPools.all || []), ...(tierPools[rerollTime] || [])];
        if (rerollPool.length || index === 0) break;
        index--;
      } while (index >= 0);
      assert.ok(rerollPool.length, 'pinned Plains pool is non-empty after tier downgrade');
      const id = rerollPool.length > 1 ? rerollPool[rerollRng.int(rerollPool.length)] : rerollPool[0];
      const species = speciesById.get(id);
      assert.ok(species, `canonical species exists for Plains pool member ${id}`);
      const legendLike = species.rarity.legendary === true || species.rarity.subLegendary === true || species.rarity.mythical === true;
      const incompatible = legendLike && (species.baseTotal >= 660 ? 1 < 80 : 1 < 55);
      if (incompatible && attempt < 10) { expectedRerolls++; continue; }
      expectedFinalSpecies = id;
      break;
    }
    const rerollMemory = new Uint8Array(api.memory.buffer);
    const rerollPtr = api.harness_legend_reroll_species_id();
    let rerollEnd = rerollPtr; while (rerollMemory[rerollEnd]) rerollEnd++;
    assert.equal(expectedRerolls, 1, 'independent source trace exercises exactly one high-BST LegendLike retry');
    assert.equal(expectedFinalSpecies, 'zubat', 'pinned retry trace resolves to the next real Plains pool member');
    assert.equal(api.harness_legend_reroll_count(), expectedRerolls, 'native Arena resolver retries high-BST legendary candidates');
    assert.equal(new TextDecoder().decode(rerollMemory.subarray(rerollPtr, rerollEnd)), expectedFinalSpecies);
  });
}
