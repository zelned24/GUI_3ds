#include "game/PokerogueRngAdapter.hpp"
#include "game/PokerogueBattleRng.hpp"
#include "game/PokerogueEncounterResolver.hpp"

namespace {
const uint16_t kSeed[] = {'p','o','k','e','r','o','g','u','e','-','r','n','g','-','v','1'};
const uint16_t kRerollSeed[] = {'r','e','r','o','l','l','-','1','4','6'};
const uint16_t kEvolutionSeed[] = {'e','v','o','-','1'};
const uint16_t kDoubleSeed[] = {'d','o','u','b','l','e','-','0'};
struct Wave1Trace {
    uint8_t cycleOffset;
    uint8_t timeOfDay;
    uint8_t doubleRoll;
    uint8_t doubleBattle;
    Pokerogue3DS::PokeroguePoolResolution pool;
};
Wave1Trace makeWave1Trace() {
    Wave1Trace trace{};
    Pokerogue3DS::PokerogueWaveClock::deriveCycleOffset(
        kSeed, sizeof(kSeed) / sizeof(kSeed[0]), trace.cycleOffset);
    const auto time = Pokerogue3DS::PokerogueWaveClock::timeOfDay(1, trace.cycleOffset);
    trace.timeOfDay = static_cast<uint8_t>(time);
    Pokerogue3DS::PokerogueRngAdapter rng;
    Pokerogue3DS::PokerogueSeedOffsetScope wave(rng, kSeed,
        sizeof(kSeed) / sizeof(kSeed[0]), 1);
    trace.doubleRoll = static_cast<uint8_t>(rng.randSeedInt(8));
    trace.doubleBattle = trace.doubleRoll == 0;
    trace.pool = Pokerogue3DS::PokerogueEncounterResolver::resolveNonBoss("town", time, 1, rng);
    return trace;
}
Pokerogue3DS::PokeroguePoolResolution makeLegendRerollTrace() {
    uint8_t offset = 0;
    Pokerogue3DS::PokerogueWaveClock::deriveCycleOffset(
        kRerollSeed, sizeof(kRerollSeed) / sizeof(kRerollSeed[0]), offset);
    const auto time = Pokerogue3DS::PokerogueWaveClock::timeOfDay(1, offset);
    Pokerogue3DS::PokerogueRngAdapter rng;
    Pokerogue3DS::PokerogueSeedOffsetScope wave(rng, kRerollSeed,
        sizeof(kRerollSeed) / sizeof(kRerollSeed[0]), 1);
    (void)rng.randSeedInt(8);
    return Pokerogue3DS::PokerogueEncounterResolver::resolveNonBoss("plains", time, 1, rng);
}
Pokerogue3DS::PokerogueRngAdapter makeSeeded() {
    Pokerogue3DS::PokerogueRngAdapter rng;
    rng.sow(kSeed, sizeof(kSeed) / sizeof(kSeed[0]));
    return rng;
}
struct Wave1DoubleTrace {
    uint8_t doubleRoll;
    uint16_t firstLevel;
    uint16_t secondLevel;
    const char* firstSpecies;
    const char* secondSpecies;
};
Wave1DoubleTrace makeWave1DoubleTrace() {
    Wave1DoubleTrace trace{};
    uint8_t offset = 0;
    Pokerogue3DS::PokerogueWaveClock::deriveCycleOffset(kDoubleSeed, 8, offset);
    const auto time = Pokerogue3DS::PokerogueWaveClock::timeOfDay(1, offset);
    Pokerogue3DS::PokerogueRngAdapter levelRng;
    uint16_t waveSeed[8]{};
    Pokerogue3DS::PokerogueRngAdapter::shiftCharCodes(kDoubleSeed, 8, 1, waveSeed, 8);
    Pokerogue3DS::PokerogueSeedOffsetScope levelScope(levelRng, waveSeed, 8, 8);
    for (uint8_t i = 0; i < 16; ++i) (void)levelRng.randSeedInt(62);
    trace.firstLevel = Pokerogue3DS::PokerogueEncounterResolver::nonBossLevelForWave(1, levelRng);
    trace.secondLevel = Pokerogue3DS::PokerogueEncounterResolver::nonBossLevelForWave(1, levelRng);

    Pokerogue3DS::PokerogueRngAdapter encounterRng;
    Pokerogue3DS::PokerogueSeedOffsetScope encounterScope(encounterRng, kDoubleSeed, 8, 1);
    trace.doubleRoll = static_cast<uint8_t>(encounterRng.randSeedInt(8));
    const auto firstPool = Pokerogue3DS::PokerogueEncounterResolver::resolveNonBoss("town", time, 1, encounterRng);
    trace.firstSpecies = firstPool.valid ? Pokerogue3DS::PokerogueEncounterResolver::resolveWildSpeciesForLevel(firstPool.speciesId, trace.firstLevel, true, encounterRng) : nullptr;
    const auto secondPool = Pokerogue3DS::PokerogueEncounterResolver::resolveNonBoss("town", time, 1, encounterRng);
    trace.secondSpecies = secondPool.valid ? Pokerogue3DS::PokerogueEncounterResolver::resolveWildSpeciesForLevel(secondPool.speciesId, trace.secondLevel, true, encounterRng) : nullptr;
    return trace;
}
}

extern "C" {
double harness_pokerogue_rng_fraction() {
    auto rng = makeSeeded();
    return rng.frac();
}

int32_t harness_pokerogue_rng_range_511() {
    auto rng = makeSeeded();
    return rng.randSeedInt(512);
}

int32_t harness_pokerogue_rng_wave1_range_511() {
    Pokerogue3DS::PokerogueRngAdapter rng;
    Pokerogue3DS::PokerogueSeedOffsetScope scope(rng, kSeed, sizeof(kSeed) / sizeof(kSeed[0]), 1);
    return scope.valid() ? rng.randSeedInt(512) : -1;
}

int32_t harness_pokerogue_rng_offset4_range_99() {
    Pokerogue3DS::PokerogueRngAdapter rng;
    Pokerogue3DS::PokerogueSeedOffsetScope scope(rng, kSeed, sizeof(kSeed) / sizeof(kSeed[0]), 4);
    return scope.valid() ? rng.randSeedInt(100) : -1;
}

int32_t harness_pokerogue_rng_seed_offset0_range_7() {
    Pokerogue3DS::PokerogueRngAdapter rng;
    Pokerogue3DS::PokerogueSeedOffsetScope scope(rng, kSeed, sizeof(kSeed) / sizeof(kSeed[0]), 0);
    return scope.valid() ? rng.randSeedInt(8) : -1;
}

int32_t harness_pokerogue_rng_singleton_is_no_draw() {
    auto rng = makeSeeded();
    const auto before = rng.state();
    const int32_t index = rng.pickIndex(1);
    const auto after = rng.state();
    return index == 0 && before.carry == after.carry && before.s0 == after.s0
        && before.s1 == after.s1 && before.s2 == after.s2;
}

int32_t harness_pokerogue_rng_empty_pick_is_rejected() {
    auto rng = makeSeeded();
    const auto before = rng.state();
    const int32_t index = rng.pickIndex(0);
    const auto after = rng.state();
    return index == -1 && before.carry == after.carry && before.s0 == after.s0
        && before.s1 == after.s1 && before.s2 == after.s2;
}

int32_t harness_pokerogue_rng_range_one_is_no_draw() {
    auto rng = makeSeeded();
    const auto before = rng.state();
    const int32_t value = rng.randSeedInt(1, 7);
    const auto after = rng.state();
    return value == 7 && before.carry == after.carry && before.s0 == after.s0
        && before.s1 == after.s1 && before.s2 == after.s2;
}

int32_t harness_pokerogue_rng_state_roundtrip() {
    auto rng = makeSeeded();
    const auto saved = rng.state();
    const int32_t first = rng.randSeedInt(512);
    rng.restore(saved);
    return first == rng.randSeedInt(512);
}

int32_t harness_pokerogue_rng_offset_restores_state() {
    auto rng = makeSeeded();
    const auto before = rng.state();
    {
        Pokerogue3DS::PokerogueSeedOffsetScope scope(rng, kSeed, sizeof(kSeed) / sizeof(kSeed[0]), 1);
        if (!scope.valid()) return 0;
        (void)rng.randSeedInt(512);
    }
    const auto after = rng.state();
    return before.carry == after.carry && before.s0 == after.s0
        && before.s1 == after.s1 && before.s2 == after.s2;
}

int32_t harness_pokerogue_shift_wraps_utf16() {
    const uint16_t input[] = {0xffff};
    uint16_t output[1] = {0};
    return Pokerogue3DS::PokerogueRngAdapter::shiftCharCodes(input, 1, 1, output, 1)
        && output[0] == 0;
}

int32_t harness_pokerogue_wave_cycle_offset() {
    uint8_t offset = 0;
    return Pokerogue3DS::PokerogueWaveClock::deriveCycleOffset(
        kSeed, sizeof(kSeed) / sizeof(kSeed[0]), offset) && offset == 25;
}

int32_t harness_pokerogue_time_of_day_boundaries() {
    using Pokerogue3DS::PokerogueTimeOfDay;
    using Pokerogue3DS::PokerogueWaveClock;
    return PokerogueWaveClock::timeOfDay(1, 13) == PokerogueTimeOfDay::Day  // cycle 14
        && PokerogueWaveClock::timeOfDay(1, 14) == PokerogueTimeOfDay::Dusk // cycle 15
        && PokerogueWaveClock::timeOfDay(1, 18) == PokerogueTimeOfDay::Dusk // cycle 19
        && PokerogueWaveClock::timeOfDay(1, 19) == PokerogueTimeOfDay::Night // cycle 20
        && PokerogueWaveClock::timeOfDay(1, 33) == PokerogueTimeOfDay::Night // cycle 34
        && PokerogueWaveClock::timeOfDay(1, 34) == PokerogueTimeOfDay::Dawn // cycle 35
        && PokerogueWaveClock::timeOfDay(1, 38) == PokerogueTimeOfDay::Dawn // cycle 39
        && PokerogueWaveClock::timeOfDay(1, 39) == PokerogueTimeOfDay::Day; // wraps to 0
}

uint32_t harness_wave1_cycle_offset() { return makeWave1Trace().cycleOffset; }
uint32_t harness_wave1_time_of_day() { return makeWave1Trace().timeOfDay; }
uint32_t harness_wave1_double_roll() { return makeWave1Trace().doubleRoll; }
uint32_t harness_wave1_double_battle() { return makeWave1Trace().doubleBattle; }
uint32_t harness_wave1_tier_roll() { return makeWave1Trace().pool.tierRoll; }
uint32_t harness_wave1_member_index() { return makeWave1Trace().pool.memberIndex; }
uint32_t harness_wave1_pool_size() { return makeWave1Trace().pool.poolSize; }
uint32_t harness_wave1_legend_rerolls() { return makeWave1Trace().pool.legendRerolls; }
const char* harness_wave1_species_id() { return makeWave1Trace().pool.speciesId; }
const char* harness_wave1_resolved_species_id(uint32_t level) {
    uint8_t offset = 0;
    Pokerogue3DS::PokerogueWaveClock::deriveCycleOffset(kSeed, 16, offset);
    const auto time = Pokerogue3DS::PokerogueWaveClock::timeOfDay(1, offset);
    Pokerogue3DS::PokerogueRngAdapter rng;
    Pokerogue3DS::PokerogueSeedOffsetScope scope(rng, kSeed, 16, 1);
    (void)rng.randSeedInt(8);
    const auto pool = Pokerogue3DS::PokerogueEncounterResolver::resolveNonBoss("town", time, 1, rng);
    return pool.valid ? Pokerogue3DS::PokerogueEncounterResolver::resolveWildSpeciesForLevel(pool.speciesId, level, true, rng) : nullptr;
}
double harness_wave1_next_fraction_after_tier() {
    Pokerogue3DS::PokerogueRngAdapter rng;
    Pokerogue3DS::PokerogueSeedOffsetScope wave(rng, kSeed,
        sizeof(kSeed) / sizeof(kSeed[0]), 1);
    (void)rng.randSeedInt(8);
    (void)rng.randSeedInt(512);
    return rng.frac();
}
double harness_wave1_state_s0_after_tier() { Pokerogue3DS::PokerogueRngAdapter rng; Pokerogue3DS::PokerogueSeedOffsetScope scope(rng,kSeed,16,1); (void)rng.randSeedInt(8); (void)rng.randSeedInt(512); return rng.state().s0; }
double harness_wave1_state_s1_after_tier() { Pokerogue3DS::PokerogueRngAdapter rng; Pokerogue3DS::PokerogueSeedOffsetScope scope(rng,kSeed,16,1); (void)rng.randSeedInt(8); (void)rng.randSeedInt(512); return rng.state().s1; }
double harness_wave1_state_s2_after_tier() { Pokerogue3DS::PokerogueRngAdapter rng; Pokerogue3DS::PokerogueSeedOffsetScope scope(rng,kSeed,16,1); (void)rng.randSeedInt(8); (void)rng.randSeedInt(512); return rng.state().s2; }
double harness_wave1_state_c_after_tier() { Pokerogue3DS::PokerogueRngAdapter rng; Pokerogue3DS::PokerogueSeedOffsetScope scope(rng,kSeed,16,1); (void)rng.randSeedInt(8); (void)rng.randSeedInt(512); return rng.state().carry; }
double harness_wave1_state_s0_after_double() { Pokerogue3DS::PokerogueRngAdapter rng; Pokerogue3DS::PokerogueSeedOffsetScope scope(rng,kSeed,16,1); (void)rng.randSeedInt(8); return rng.state().s0; }
double harness_wave1_state_s1_after_double() { Pokerogue3DS::PokerogueRngAdapter rng; Pokerogue3DS::PokerogueSeedOffsetScope scope(rng,kSeed,16,1); (void)rng.randSeedInt(8); return rng.state().s1; }
double harness_wave1_state_s2_after_double() { Pokerogue3DS::PokerogueRngAdapter rng; Pokerogue3DS::PokerogueSeedOffsetScope scope(rng,kSeed,16,1); (void)rng.randSeedInt(8); return rng.state().s2; }
double harness_wave1_state_c_after_double() { Pokerogue3DS::PokerogueRngAdapter rng; Pokerogue3DS::PokerogueSeedOffsetScope scope(rng,kSeed,16,1); (void)rng.randSeedInt(8); return rng.state().carry; }
double harness_wave1_first_fraction() { Pokerogue3DS::PokerogueRngAdapter rng; Pokerogue3DS::PokerogueSeedOffsetScope scope(rng,kSeed,16,1); return rng.frac(); }
uint32_t harness_wave1_non_boss_level() {
    Pokerogue3DS::PokerogueRngAdapter rng;
    uint16_t waveSeed[16]{};
    Pokerogue3DS::PokerogueRngAdapter::shiftCharCodes(kSeed, 16, 1, waveSeed, 16);
    Pokerogue3DS::PokerogueSeedOffsetScope scope(rng, waveSeed, 16, 8);
    for (uint8_t i = 0; i < 16; ++i) (void)rng.randSeedInt(62);
    return Pokerogue3DS::PokerogueEncounterResolver::nonBossLevelForWave(1, rng);
}
uint32_t harness_wave11_non_boss_level() {
    Pokerogue3DS::PokerogueRngAdapter rng;
    uint16_t waveSeed[16]{};
    Pokerogue3DS::PokerogueRngAdapter::shiftCharCodes(kSeed, 16, 11, waveSeed, 16);
    Pokerogue3DS::PokerogueSeedOffsetScope scope(rng, waveSeed, 16, 88);
    for (uint8_t i = 0; i < 16; ++i) (void)rng.randSeedInt(62);
    return Pokerogue3DS::PokerogueEncounterResolver::nonBossLevelForWave(11, rng);
}
uint32_t harness_legend_reroll_count() { return makeLegendRerollTrace().legendRerolls; }
const char* harness_legend_reroll_species_id() { return makeLegendRerollTrace().speciesId; }
const char* harness_forced_prevolution_species() {
    auto rng = makeSeeded();
    return Pokerogue3DS::PokerogueEncounterResolver::resolveWildSpeciesForLevel("venusaur", 17, false, rng);
}
const char* harness_level_evolution_species() {
    Pokerogue3DS::PokerogueRngAdapter rng;
    rng.sow(kEvolutionSeed, sizeof(kEvolutionSeed) / sizeof(kEvolutionSeed[0]));
    return Pokerogue3DS::PokerogueEncounterResolver::resolveWildSpeciesForLevel("bulbasaur", 18, true, rng);
}
uint32_t harness_test_double_wave1_roll() { return makeWave1DoubleTrace().doubleRoll; }
const char* harness_battle_seed_wave1() {
    static char output[Pokerogue3DS::PokerogueBattleRng::kBattleSeedLength + 1];
    Pokerogue3DS::PokerogueBattleRng battle;
    if (!battle.initialize(kSeed, 16, 1)) return "";
    for (std::size_t i = 0; i < Pokerogue3DS::PokerogueBattleRng::kBattleSeedLength; ++i)
        output[i] = static_cast<char>(battle.battleSeed()[i]);
    output[Pokerogue3DS::PokerogueBattleRng::kBattleSeedLength] = '\0';
    return output;
}
uint32_t harness_battle_rng_wave1_turn(uint32_t turn) {
    Pokerogue3DS::PokerogueBattleRng battle;
    if (!battle.initialize(kSeed, 16, 1) || !battle.beginTurn(turn)) return 0xffffffffu;
    int32_t output = -1;
    return battle.randSeedInt(100, output) ? static_cast<uint32_t>(output) : 0xffffffffu;
}
uint32_t harness_wave1_double_first_level() { return makeWave1DoubleTrace().firstLevel; }
uint32_t harness_wave1_double_second_level() { return makeWave1DoubleTrace().secondLevel; }
const char* harness_wave1_double_first_species() { return makeWave1DoubleTrace().firstSpecies; }
const char* harness_wave1_double_second_species() { return makeWave1DoubleTrace().secondSpecies; }
}
