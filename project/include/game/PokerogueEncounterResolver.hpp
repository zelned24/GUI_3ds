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
    uint8_t legendRerolls;
    bool valid;
};

class PokerogueEncounterResolver {
public:
    // Ports the pinned determineEnemySpecies level heuristic for wild
    // encounters. The caller owns the upstream RNG scope and call ordering.
    static const char* resolveWildSpeciesForLevel(const char* speciesId,
        uint16_t level, bool allowEvolving, PokerogueRngAdapter& rng) {
        if (!speciesId) return nullptr;
        char prevo[64][48]{};
        uint16_t prevoThreshold[64]{};
        uint16_t count = 0;
        collectPrevolutions(speciesId, prevo, prevoThreshold, count, 0);
        for (int32_t i = static_cast<int32_t>(count) - 1; i >= 0; --i) {
            if (level < prevoThreshold[i]) {
                const auto* required = findSpecies(prevo[i]);
                return required ? required->id : nullptr;
            }
        }
        if (!allowEvolving) return speciesId;

        const PokerogueContent::SpeciesEvolution* eligible[32]{};
        uint16_t eligibleCount = 0;
        for (std::size_t i = 0; i < PokerogueContent::kSpeciesEvolutionCount; ++i) {
            const auto& edge = PokerogueContent::kSpeciesEvolutions[i];
            if (!same(edge.sourceSpeciesId, speciesId)) continue;
            const uint16_t threshold = edge.wildThreshold < 0 ? 0 : static_cast<uint16_t>(edge.wildThreshold);
            const uint16_t required = threshold > edge.level ? threshold : edge.level;
            if (level < edge.level || level < required) continue;
            if (eligibleCount >= 32) return nullptr;
            eligible[eligibleCount++] = &edge;
        }
        if (!eligibleCount) return speciesId;
        const auto* selected = eligible[rng.pickIndex(eligibleCount)];
        const uint16_t choice = selected->wildThreshold < 0
            ? selected->level
            : (static_cast<uint16_t>(selected->wildThreshold) > selected->level
                ? static_cast<uint16_t>(selected->wildThreshold) : selected->level);
        const uint16_t randomMax = static_cast<uint16_t>(choice * 1.2 + 0.5);
        const int32_t randomLevel = rng.randSeedIntRange(choice, randomMax);
        return randomLevel <= level
            ? resolveWildSpeciesForLevel(selected->targetSpeciesId, level, true, rng)
            : speciesId;
    }

    // Ports the pinned Arena non-boss tier thresholds, empty-tier downgrade,
    // ALL-then-time pool composition and randSeedItem member draw. Callers own
    // stream setup and preceding upstream draws (wave-1 double check).
    static PokeroguePoolResolution resolveNonBoss(const char* biomeId,
        PokerogueTimeOfDay time, uint32_t adjustedWave, PokerogueRngAdapter& rng) {
        static const char* const tiers[] = {
            "common", "uncommon", "rare", "super_rare", "ultra_rare"
        };
        const char* timeId = time == PokerogueTimeOfDay::Day ? "day"
            : time == PokerogueTimeOfDay::Dusk ? "dusk"
            : time == PokerogueTimeOfDay::Night ? "night" : "dawn";
        for (uint8_t attempt = 0; attempt <= 10; ++attempt) {
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
            if (!count) return {nullptr, tiers[requested], tiers[tierIndex], 0, 0, roll, attempt, false};
            const uint16_t selected = static_cast<uint16_t>(rng.pickIndex(count));
            const char* speciesId = memberAt(biomeId, tiers[tierIndex], timeId, selected);
            const auto* species = findSpecies(speciesId);
            if (!species || !species->baseTotal) {
                return {speciesId, tiers[requested], tiers[tierIndex], count, selected, roll, attempt, false};
            }
            // Pinned PokemonSpecies constructor defaults omitted rarity flags
            // to false. The canonical record still preserves their null/raw
            // origin, while runtime follows that explicit upstream default.
            const bool legendLike = species->legendary == 1 || species->subLegendary == 1
                || species->mythical == 1;
            const bool incompatible = legendLike
                && (species->baseTotal >= 660 ? adjustedWave < 80 : adjustedWave < 55);
            if (incompatible && attempt < 10) continue;
            return {speciesId, tiers[requested], tiers[tierIndex], count, selected,
                    roll, attempt, speciesId != nullptr};
        }
        return {nullptr, "unsupported", "unsupported", 0, 0, 0, 10, false};
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
    static void collectPrevolutions(const char* speciesId, char prevo[][48],
        uint16_t thresholds[], uint16_t& count, uint8_t depth) {
        if (depth >= 32 || count >= 64) return;
        for (std::size_t i = 0; i < PokerogueContent::kSpeciesEvolutionCount; ++i) {
            const auto& edge = PokerogueContent::kSpeciesEvolutions[i];
            if (!same(edge.targetSpeciesId, speciesId)) continue;
            bool duplicate = false;
            for (uint16_t j = 0; j < count; ++j) duplicate |= same(prevo[j], edge.sourceSpeciesId);
            if (duplicate) continue;
            uint16_t threshold = edge.wildThreshold < 0 ? edge.level : static_cast<uint16_t>(edge.wildThreshold);
            if (edge.level == 1) threshold = edge.wildThreshold < 0 ? 1 : static_cast<uint16_t>(edge.wildThreshold);
            else if (edge.wildThreshold < 0 || edge.level < threshold) threshold = edge.level;
            copyId(prevo[count], edge.sourceSpeciesId);
            thresholds[count++] = threshold;
            collectPrevolutions(edge.sourceSpeciesId, prevo, thresholds, count, depth + 1);
        }
    }

    static void copyId(char destination[48], const char* source) {
        std::size_t i = 0;
        while (source && source[i] && i < 47) { destination[i] = source[i]; ++i; }
        destination[i] = '\0';
    }

    static const PokerogueContent::Species* findSpecies(const char* speciesId) {
        if (!speciesId) return nullptr;
        for (std::size_t i = 0; i < PokerogueContent::kSpeciesCount; ++i) {
            if (same(PokerogueContent::kSpecies[i].id, speciesId)) return &PokerogueContent::kSpecies[i];
        }
        return nullptr;
    }

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
