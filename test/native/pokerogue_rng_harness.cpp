#include "game/PokerogueRngAdapter.hpp"
#include "game/PokerogueEncounterResolver.hpp"

namespace {
const uint16_t kSeed[] = {'p','o','k','e','r','o','g','u','e','-','r','n','g','-','v','1'};
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
    trace.pool = Pokerogue3DS::PokerogueEncounterResolver::resolveNonBoss("town", time, rng);
    return trace;
}
Pokerogue3DS::PokerogueRngAdapter makeSeeded() {
    Pokerogue3DS::PokerogueRngAdapter rng;
    rng.sow(kSeed, sizeof(kSeed) / sizeof(kSeed[0]));
    return rng;
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
const char* harness_wave1_species_id() { return makeWave1Trace().pool.speciesId; }
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
    Pokerogue3DS::PokerogueSeedOffsetScope scope(rng, kSeed, 16, 8);
    for (uint8_t i = 0; i < 16; ++i) (void)rng.randSeedInt(62);
    return Pokerogue3DS::PokerogueEncounterResolver::nonBossLevelForWave(1, rng);
}
}
