#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include <cstdint>

namespace Pokerogue3DS {

struct PokeroguePoolResolution {
    const char* speciesId;
    const char* requestedTier;
    const char* selectedTier;
    uint16_t poolSize;
    uint16_t memberIndex;
    uint16_t tierRoll;
    bool valid;
};

class PokerogueEncounterResolver {
public:
    // Ports the pinned Arena non-boss tier thresholds, empty-tier downgrade,
    // ALL-then-time pool composition and randSeedItem member draw. Callers own
    // stream setup and preceding upstream draws (wave-1 double check).
    static PokeroguePoolResolution resolveNonBoss(const char* biomeId,
        PokerogueTimeOfDay time, PokerogueRngAdapter& rng) {
        static const char* const tiers[] = {
            "common", "uncommon", "rare", "super_rare", "ultra_rare"
        };
        const char* timeId = time == PokerogueTimeOfDay::Day ? "day"
            : time == PokerogueTimeOfDay::Dusk ? "dusk"
            : time == PokerogueTimeOfDay::Night ? "night" : "dawn";
        const uint16_t roll = static_cast<uint16_t>(rng.randSeedInt(512));
        uint8_t tierIndex = roll >= 156 ? 0 : roll >= 32 ? 1
            : roll >= 6 ? 2 : roll >= 1 ? 3 : 4;
        const uint8_t requested = tierIndex;
        uint16_t count = 0;
        for (;;) {
            count = countMembers(biomeId, tiers[tierIndex], timeId);
            if (count || tierIndex == 0) break;
            --tierIndex;
        }
        if (!count) return {nullptr, tiers[requested], tiers[tierIndex], 0, 0, roll, false};
        const uint16_t selected = static_cast<uint16_t>(rng.pickIndex(count));
        const char* species = memberAt(biomeId, tiers[tierIndex], timeId, selected);
        return {species, tiers[requested], tiers[tierIndex], count, selected, roll, species != nullptr};
    }

    // Battle.getLevelForWave non-boss branch for Classic difficulty waves.
    // Caller supplies the battle-scoped RNG after Battle's 16-char battleSeed
    // initializer has consumed its randSeedInt(62) draws.
    static uint16_t nonBossLevelForWave(uint32_t waveIndex, PokerogueRngAdapter& rng) {
        if (!waveIndex) return 1;
        const double baseLevel = 1.0 + static_cast<double>(waveIndex) / 2.0
            + (static_cast<double>(waveIndex) / 25.0) * (static_cast<double>(waveIndex) / 25.0);
        const double deviation = 10.0 / static_cast<double>(waveIndex);
        double randomSum = 0.0;
        for (double remaining = deviation; remaining > 0.0; remaining -= 1.0) {
            randomSum += rng.realInRange(0.0, 1.0);
        }
        const double offset = randomSum / deviation;
        const double rounded = baseLevel + offset + 0.5;
        const uint32_t level = static_cast<uint32_t>(rounded);
        return static_cast<uint16_t>(level ? level : 1);
    }

private:
    static bool same(const char* left, const char* right) {
        if (!left || !right) return left == right;
        while (*left && *right && *left == *right) { ++left; ++right; }
        return *left == *right;
    }

    static uint16_t countMembers(const char* biomeId, const char* tier, const char* time) {
        uint16_t count = 0;
        for (uint8_t pass = 0; pass < 2; ++pass) {
            const char* wantedTime = pass == 0 ? "all" : time;
            for (std::size_t i = 0; i < PokerogueContent::kBiomeEncounterPoolCount; ++i) {
                const auto& entry = PokerogueContent::kBiomeEncounterPools[i];
                if (same(entry.biomeId, biomeId) && same(entry.tier, tier)
                    && same(entry.timeOfDay, wantedTime)) ++count;
            }
        }
        return count;
    }

    static const char* memberAt(const char* biomeId, const char* tier,
                               const char* time, uint16_t selected) {
        uint16_t index = 0;
        for (uint8_t pass = 0; pass < 2; ++pass) {
            const char* wantedTime = pass == 0 ? "all" : time;
            for (std::size_t i = 0; i < PokerogueContent::kBiomeEncounterPoolCount; ++i) {
                const auto& entry = PokerogueContent::kBiomeEncounterPools[i];
                if (!same(entry.biomeId, biomeId) || !same(entry.tier, tier)
                    || !same(entry.timeOfDay, wantedTime)) continue;
                if (index++ == selected) return entry.speciesId;
            }
        }
        return nullptr;
    }
};

} // namespace Pokerogue3DS
