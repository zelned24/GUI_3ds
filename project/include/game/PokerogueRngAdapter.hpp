#pragma once

#include <cstddef>
#include <cstdint>

namespace Pokerogue3DS {

// Phaser 3.90.0 RandomDataGenerator state. The seed hash uses UTF-16 code units
// because PokéRogue's shiftCharCodes derives wave/offset seeds that way.
struct PokerogueRngState {
    double carry;
    double s0;
    double s1;
    double s2;
};

enum class PokerogueTimeOfDay : uint8_t { Day, Dusk, Night, Dawn };

class PokerogueRngAdapter {
public:
    static constexpr size_t kMaxSeedCodeUnits = 64;

    PokerogueRngAdapter() : m_n(0xefc8249d), m_state{1.0, 0.0, 0.0, 0.0} {}

    void sow(const uint16_t* seed, size_t length) {
        m_n = 0xefc8249d;
        m_state.s0 = hashCodeUnit(' ');
        m_state.s1 = hashCodeUnit(' ');
        m_state.s2 = hashCodeUnit(' ');
        m_state.carry = 1.0;
        subtractSeed(m_state.s0, seed, length);
        subtractSeed(m_state.s1, seed, length);
        subtractSeed(m_state.s2, seed, length);
    }

    static bool shiftCharCodes(const uint16_t* source, size_t length,
                               uint32_t offset, uint16_t* destination,
                               size_t capacity) {
        if (length > capacity || (length && (!source || !destination))) return false;
        for (size_t i = 0; i < length; ++i) {
            destination[i] = static_cast<uint16_t>(source[i] + offset);
        }
        return true;
    }

    double frac() {
        // Phaser evaluates these draws in source order. Keep the stateful calls
        // in separate statements; operand evaluation order for `+` is not a
        // sequencing contract in C++.
        const double first = rnd();
        const int32_t secondBits = static_cast<int32_t>(rnd() * 2097152.0);
        return first + secondBits * 1.1102230246251565e-16;
    }

    double realInRange(double min, double max) {
        return frac() * (max - min) + min;
    }

    int32_t integerInRange(int32_t min, int32_t max) {
        const double value = realInRange(0.0, static_cast<double>(max) - min + 1.0) + min;
        int32_t result = static_cast<int32_t>(value);
        if (static_cast<double>(result) > value) --result; // JS Math.floor, including negative ranges.
        return result;
    }

    // Upstream randSeedInt skips RNG consumption for range <= 1.
    int32_t randSeedInt(int32_t range, int32_t min = 0) {
        return range <= 1 ? min : integerInRange(min, min + range - 1);
    }

    int32_t randSeedIntRange(int32_t min, int32_t max) {
        return max <= min ? min : randSeedInt(max - min + 1, min);
    }

    // Phaser RND.pick(array) delegates to integerInRange. PokéRogue's
    // randSeedItem short-circuits singletons before reaching Phaser.
    int32_t pickIndex(uint32_t count) {
        if (!count) return -1;
        return count == 1 ? 0 : integerInRange(0, static_cast<int32_t>(count - 1));
    }

    PokerogueRngState state() const { return m_state; }
    void restore(const PokerogueRngState& state) { m_state = state; }

private:
    double m_n;
    PokerogueRngState m_state;

    static uint32_t toUint32(double value) {
        // JavaScript's >>> 0 truncates toward zero and wraps modulo 2^32.
        // A direct floating-to-uint32_t cast is undefined when value is out
        // of range, which occurs in Phaser's hash multiplication step.
        const bool negative = value < 0.0;
        const double magnitude = negative ? -value : value;
        const uint64_t truncated = static_cast<uint64_t>(magnitude);
        const uint32_t low = static_cast<uint32_t>(truncated & 0xffffffffULL);
        return negative ? static_cast<uint32_t>(0U - low) : low;
    }

    double hashCodeUnit(uint16_t code) {
        m_n += code;
        double h = 0.02519603282416938 * m_n;
        m_n = toUint32(h); // JS >>> 0
        h -= m_n;
        h *= m_n;
        m_n = toUint32(h);
        h -= m_n;
        m_n += h * 4294967296.0;
        return toUint32(m_n) * 2.3283064365386963e-10;
    }

    double hash(const uint16_t* value, size_t length) {
        for (size_t i = 0; i < length; ++i) hashCodeUnit(value[i]);
        return toUint32(m_n) * 2.3283064365386963e-10;
    }

    void subtractSeed(double& value, const uint16_t* seed, size_t length) {
        value -= hash(seed, length);
        if (value < 0.0) value += 1.0;
    }

    double rnd() {
        const double t = 2091639.0 * m_state.s0
            + m_state.carry * 2.3283064365386963e-10;
        m_state.carry = static_cast<int32_t>(t); // JS ToInt32 (`t | 0`)
        m_state.s0 = m_state.s1;
        m_state.s1 = m_state.s2;
        m_state.s2 = t - m_state.carry;
        return m_state.s2;
    }
};

// Classic biome pool clock from Arena.getTimeOfDay(). Upstream derives one
// stable five-wave cycle offset from a fresh root-seed Phaser stream.
class PokerogueWaveClock {
public:
    static bool deriveCycleOffset(const uint16_t* rootSeed, size_t seedLength,
                                  uint8_t& offset) {
        if (seedLength > PokerogueRngAdapter::kMaxSeedCodeUnits
            || (seedLength && !rootSeed)) return false;
        PokerogueRngAdapter rng;
        rng.sow(rootSeed, seedLength);
        offset = static_cast<uint8_t>(rng.randSeedInt(8) * 5);
        return true;
    }

    static PokerogueTimeOfDay timeOfDay(uint32_t waveIndex, uint8_t cycleOffset) {
        const uint32_t cycle = (waveIndex + cycleOffset) % 40;
        if (cycle < 15) return PokerogueTimeOfDay::Day;
        if (cycle < 20) return PokerogueTimeOfDay::Dusk;
        if (cycle < 35) return PokerogueTimeOfDay::Night;
        return PokerogueTimeOfDay::Dawn;
    }
};

// Mirrors executeWithSeedOffset's save / sow / callback / restore behavior.
// State restoration intentionally excludes Phaser's internal hash accumulator;
// its public state() format also contains only c, s0, s1 and s2.
class PokerogueSeedOffsetScope {
public:
    PokerogueSeedOffsetScope(PokerogueRngAdapter& rng, const uint16_t* baseSeed,
                             size_t seedLength, uint32_t offset)
        : m_rng(rng), m_saved(rng.state()), m_active(false) {
        if (seedLength > PokerogueRngAdapter::kMaxSeedCodeUnits) return;
        uint16_t shifted[PokerogueRngAdapter::kMaxSeedCodeUnits];
        if (!PokerogueRngAdapter::shiftCharCodes(baseSeed, seedLength, offset,
                                                  shifted, PokerogueRngAdapter::kMaxSeedCodeUnits)) return;
        m_rng.sow(shifted, seedLength);
        m_active = true;
    }

    PokerogueSeedOffsetScope(const PokerogueSeedOffsetScope&) = delete;
    PokerogueSeedOffsetScope& operator=(const PokerogueSeedOffsetScope&) = delete;

    ~PokerogueSeedOffsetScope() {
        if (m_active) m_rng.restore(m_saved);
    }

    bool valid() const { return m_active; }

private:
    PokerogueRngAdapter& m_rng;
    PokerogueRngState m_saved;
    bool m_active;
};

} // namespace Pokerogue3DS
