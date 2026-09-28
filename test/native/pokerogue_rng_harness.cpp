#include "game/PokerogueRngAdapter.hpp"

namespace {
const uint16_t kSeed[] = {'p','o','k','e','r','o','g','u','e','-','r','n','g','-','v','1'};
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
}
