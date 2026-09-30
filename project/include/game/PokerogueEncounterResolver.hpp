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

struct PokerogueTrainerPoolResolution {
    const PokerogueContent::TrainerType* trainerType;
    const PokerogueContent::BiomeTrainerPoolEntry* poolEntry;
    const char* requestedTier;
    const char* selectedTier;
    uint16_t poolSize;
    uint16_t memberIndex;
    uint16_t tierRoll;
    bool valid;
};

struct PokerogueTrainerSpeciesResolution {
    const char* speciesId;
    const char* requestedTier;
    const char* selectedTier;
    uint16_t poolSize;
    uint16_t candidateIndex;
    uint16_t tierRoll;
    uint16_t memberRolls;
    bool valid;
};

class PokerogueEncounterResolver {
public:
    // Ports Arena.randomTrainerType tier rolls, empty-pool downgrade, ordered
    // member selection and the upstream BREEDER fallback. `trainerBoss` is the
    // already-resolved GameMode.isTrainerBoss result at the caller's wave.
    static PokerogueTrainerPoolResolution resolveTrainerType(
        const char* biomeId, bool trainerBoss, bool isBoss, PokerogueRngAdapter& rng) {
        if (!biomeId || !*biomeId) return {nullptr, nullptr, nullptr, nullptr, 0, 0, 0, false};
        const bool hasBossPool = countTrainerMembers(biomeId, "boss") != 0;
        const bool useBossTiers = hasBossPool && (trainerBoss || isBoss);
        const uint16_t roll = static_cast<uint16_t>(rng.randSeedInt(useBossTiers ? 64 : 512));
        const char* tier = useBossTiers
            ? (roll >= 20 ? "boss" : roll >= 6 ? "boss_rare" : roll >= 1 ? "boss_super_rare" : "boss_ultra_rare")
            : (roll >= 156 ? "common" : roll >= 32 ? "uncommon" : roll >= 6 ? "rare" : roll >= 1 ? "super_rare" : "ultra_rare");
        const char* requested = tier;
        uint16_t count = countTrainerMembers(biomeId, tier);
        while (!count && !same(tier, "common")) {
            tier = previousTrainerTier(tier);
            if (!tier) return {nullptr, nullptr, requested, nullptr, 0, 0, roll, false};
            count = countTrainerMembers(biomeId, tier);
        }
        if (count) {
            const uint16_t selected = static_cast<uint16_t>(rng.pickIndex(count));
            const auto* entry = trainerMemberAt(biomeId, tier, selected);
            const auto* trainer = entry ? PokerogueContent::findTrainerTypeByKey(entry->trainerId) : nullptr;
            return {trainer, entry, requested, tier, count, selected, roll, trainer != nullptr};
        }
        const auto* fallback = PokerogueContent::findTrainerTypeByKey("breeder");
        return {fallback, nullptr, requested, "common", 0, 0, roll, fallback != nullptr};
    }

    // Ports Trainer.genNewPartyMemberSpecies when TrainerConfig declares a
    // static speciesPools object. Source array-valued candidates are preserved
    // but upstream rerolls them until randSeedItem returns a numeric SpeciesId;
    // catalogs without any numeric candidate are explicitly unsupported.
    static PokerogueTrainerSpeciesResolution resolveTrainerPoolSpecies(
        const PokerogueContent::TrainerType& trainer, PokerogueRngAdapter& rng) {
        const int32_t roll = rng.randSeedInt(512);
        const char* tier = roll >= 156 ? "common" : roll >= 32 ? "uncommon"
            : roll >= 6 ? "rare" : roll >= 1 ? "super_rare" : "ultra_rare";
        const char* requested = tier;
        const auto* pool = PokerogueContent::trainerSpeciesPoolForTier(trainer, tier);
        while ((!pool || !pool->candidateCount) && !same(tier, "common")) {
            tier = previousTrainerTier(tier);
            if (!tier) return {nullptr, requested, nullptr, 0, 0, static_cast<uint16_t>(roll), 0, false};
            pool = PokerogueContent::trainerSpeciesPoolForTier(trainer, tier);
        }
        const auto* candidates = pool ? PokerogueContent::trainerPoolChoicesFor(*pool) : nullptr;
        if (!pool || !candidates || !pool->candidateCount) return {nullptr, requested, tier, 0, 0, static_cast<uint16_t>(roll), 0, false};
        bool hasNumericSpecies = false;
        for (uint16_t i = 0; i < pool->candidateCount; ++i) hasNumericSpecies |= !candidates[i].isGroup && candidates[i].speciesCount == 1;
        if (!hasNumericSpecies) return {nullptr, requested, tier, pool->candidateCount, 0, static_cast<uint16_t>(roll), 0, false};
        uint16_t memberRolls = 0;
        for (;;) {
            const uint16_t index = static_cast<uint16_t>(rng.pickIndex(pool->candidateCount));
            ++memberRolls;
            const auto& candidate = candidates[index];
            if (candidate.isGroup) continue;
            if (candidate.speciesCount != 1) return {nullptr, requested, tier, pool->candidateCount, index, static_cast<uint16_t>(roll), memberRolls, false};
            const auto* species = PokerogueContent::trainerPoolSpeciesFor(candidate);
            if (!species || !species->speciesId) return {nullptr, requested, tier, pool->candidateCount, index, static_cast<uint16_t>(roll), memberRolls, false};
            return {species->speciesId, requested, tier, pool->candidateCount, index, static_cast<uint16_t>(roll), memberRolls, true};
        }
    }

    // Ports the trainer branch of determineEnemySpecies for source-derived
    // NORMAL/STRONG evolution thresholds. The caller owns the trainer RNG
    // stream and supplies the current Classic trainer-wave suppression fact.
    // allowEvolving=false still permits a required prevolution, matching the
    // same-species member branch of Trainer.genPartyMember.
    static const char* resolveTrainerSpeciesForLevel(const char* speciesId,
        uint16_t level, uint8_t evolutionThresholdKindId, bool classicTrainerWave20,
        PokerogueRngAdapter& rng, bool tryForcePrevo = true, bool allowEvolving = true) {
        if (!speciesId || level == 0 || evolutionThresholdKindId > 1) return nullptr;
        char prevo[64][48]{};
        uint16_t prevoThreshold[64]{};
        uint16_t count = 0;
        if (tryForcePrevo) collectTrainerPrevolutions(speciesId, evolutionThresholdKindId, prevo,
            prevoThreshold, count, 0);
        for (int32_t i = static_cast<int32_t>(count) - 1; i >= 0; --i)
            if (level < prevoThreshold[i]) {
                const auto* required = findSpecies(prevo[i]);
                return required ? required->id : nullptr;
            }
        if (!allowEvolving || classicTrainerWave20) return speciesId;

        const PokerogueContent::SpeciesEvolution* eligible[32]{};
        uint16_t eligibleCount = 0;
        for (std::size_t i = 0; i < PokerogueContent::kSpeciesEvolutionCount; ++i) {
            const auto& edge = PokerogueContent::kSpeciesEvolutions[i];
            if (!same(edge.sourceSpeciesId, speciesId)) continue;
            const int16_t sourceThreshold = evolutionThresholdKindId == 0
                ? edge.strongThreshold : edge.normalThreshold;
            const uint16_t threshold = sourceThreshold < 0 ? edge.level
                : (static_cast<uint16_t>(sourceThreshold) > edge.level
                    ? static_cast<uint16_t>(sourceThreshold) : edge.level);
            if (!threshold || level < edge.level || level < threshold) continue;
            if (eligibleCount >= 32) return nullptr;
            eligible[eligibleCount++] = &edge;
        }
        if (!eligibleCount) return speciesId;
        const auto* selected = eligible[rng.pickIndex(eligibleCount)];
        const int16_t sourceThreshold = evolutionThresholdKindId == 0
            ? selected->strongThreshold : selected->normalThreshold;
        const uint16_t choice = sourceThreshold < 0 ? selected->level
            : (static_cast<uint16_t>(sourceThreshold) > selected->level
                ? static_cast<uint16_t>(sourceThreshold) : selected->level);
        const uint16_t maxLevel = evolutionThresholdKindId == 0
            ? choice : static_cast<uint16_t>((choice * 11 + 5) / 10);
        const int32_t randomLevel = rng.randSeedIntRange(choice, maxLevel);
        return randomLevel <= level
            ? resolveTrainerSpeciesForLevel(selected->targetSpeciesId, level,
                evolutionThresholdKindId, classicTrainerWave20, rng, false, allowEvolving)
            : speciesId;
    }

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
    static uint16_t trainerEvolutionThreshold(const PokerogueContent::SpeciesEvolution& edge,
        uint8_t evolutionThresholdKindId) {
        const int16_t sourceThreshold = evolutionThresholdKindId == 0
            ? edge.strongThreshold : edge.normalThreshold;
        return sourceThreshold < 0 ? edge.level : static_cast<uint16_t>(sourceThreshold);
    }

    static void collectTrainerPrevolutions(const char* speciesId, uint8_t evolutionThresholdKindId,
        char prevo[][48], uint16_t thresholds[], uint16_t& count, uint8_t depth) {
        if (depth >= 32 || count >= 64) return;
        for (std::size_t i = 0; i < PokerogueContent::kSpeciesEvolutionCount; ++i) {
            const auto& edge = PokerogueContent::kSpeciesEvolutions[i];
            if (!same(edge.targetSpeciesId, speciesId)) continue;
            bool duplicate = false;
            for (uint16_t j = 0; j < count; ++j) duplicate |= same(prevo[j], edge.sourceSpeciesId);
            if (duplicate) continue;
            const uint16_t threshold = trainerEvolutionThreshold(edge, evolutionThresholdKindId);
            const uint16_t required = edge.level == 1 ? threshold
                : (edge.level < threshold ? edge.level : threshold);
            copyId(prevo[count], edge.sourceSpeciesId);
            thresholds[count++] = required;
            collectTrainerPrevolutions(edge.sourceSpeciesId, evolutionThresholdKindId,
                prevo, thresholds, count, depth + 1);
        }
    }

    static const char* previousTrainerTier(const char* tier) {
        if (same(tier, "boss_ultra_rare")) return "boss_super_rare";
        if (same(tier, "boss_super_rare")) return "boss_rare";
        if (same(tier, "boss_rare")) return "boss";
        if (same(tier, "boss")) return "ultra_rare";
        if (same(tier, "ultra_rare")) return "super_rare";
        if (same(tier, "super_rare")) return "rare";
        if (same(tier, "rare")) return "uncommon";
        if (same(tier, "uncommon")) return "common";
        return nullptr;
    }

    static uint16_t countTrainerMembers(const char* biomeId, const char* tier) {
        uint16_t count = 0;
        for (std::size_t i = 0; i < PokerogueContent::kBiomeTrainerPoolCount; ++i) {
            const auto& entry = PokerogueContent::kBiomeTrainerPools[i];
            if (same(entry.biomeId, biomeId) && same(entry.tier, tier)) ++count;
        }
        return count;
    }

    static const PokerogueContent::BiomeTrainerPoolEntry* trainerMemberAt(
        const char* biomeId, const char* tier, uint16_t index) {
        for (std::size_t i = 0; i < PokerogueContent::kBiomeTrainerPoolCount; ++i) {
            const auto& entry = PokerogueContent::kBiomeTrainerPools[i];
            if (same(entry.biomeId, biomeId) && same(entry.tier, tier)) {
                if (!index) return &entry;
                --index;
            }
        }
        return nullptr;
    }

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
