#pragma once

#include "game/PokerogueRngAdapter.hpp"
#include "content/PokerogueRuntimeContent.hpp"

namespace Pokerogue3DS {

inline bool sameBiomeId(const char* left, const char* right) {
    if (!left || !right) return false;
    while (*left && *left == *right) { ++left; ++right; }
    return *left == *right;
}

enum class ClassicBiomeTransitionResult : uint8_t {
    Ok = 0,
    InvalidInput,
    UnsupportedMode,
    MissingBiome,
    MissingRoute,
    NoEligibleRoute,
    AwaitingMapChoice,
    InvalidMapChoice,
};

// Resolves the data and RNG part of upstream SelectBiomePhase for Classic.
// The caller must pass nextWaveIndex (current wave + 1) and the run's root seed;
// SelectBiomePhase calls resetSeed() before evaluating biomeLinks.
// MapModifier ownership/UI is external: when it is present and several links
// survive their exclusion rolls, the caller receives AwaitingMapChoice and
// calls again with a selected destination from that eligible set.
inline ClassicBiomeTransitionResult resolveClassicNextBiome(
    const char* currentBiomeId,
    uint32_t nextWaveIndex,
    bool isClassic,
    const uint16_t* rootSeed,
    size_t seedLength,
    bool mapModifierAvailable,
    const char* selectedDestination,
    const char*& outputBiomeId) {
    outputBiomeId = nullptr;
    if (!currentBiomeId || !*currentBiomeId || seedLength > PokerogueRngAdapter::kMaxSeedCodeUnits ||
        (seedLength && !rootSeed)) return ClassicBiomeTransitionResult::InvalidInput;
    if (!isClassic) return ClassicBiomeTransitionResult::UnsupportedMode;

    // SelectBiomePhase routes to END ten waves before the Classic final boss.
    if (nextWaveIndex <= 200 && nextWaveIndex + 9 == 200) {
        for (const auto& biome : PokerogueContent::kBiomes) {
            if (sameBiomeId(biome.id, "end")) {
                outputBiomeId = biome.id;
                return ClassicBiomeTransitionResult::Ok;
            }
        }
        return ClassicBiomeTransitionResult::MissingBiome;
    }

    const PokerogueContent::Route* first = PokerogueContent::routesFrom(currentBiomeId);
    if (!first) return ClassicBiomeTransitionResult::MissingRoute;
    const PokerogueContent::Route* second = PokerogueContent::routesFrom(currentBiomeId, 1);
    if (!second) {
        // The upstream single-link branch directly returns the sole link and
        // does not perform a weighted exclusion roll.
        outputBiomeId = first->to;
        return ClassicBiomeTransitionResult::Ok;
    }

    PokerogueRngAdapter rng;
    rng.sow(rootSeed, seedLength);
    const PokerogueContent::Route* eligible[PokerogueContent::kRouteCount]{};
    size_t eligibleCount = 0;
    for (size_t ordinal = 0; ordinal < PokerogueContent::kRouteCount; ++ordinal) {
        const auto* route = PokerogueContent::routesFrom(currentBiomeId, ordinal);
        if (!route) break;
        // Upstream BiomeLinks weight is an exclusion denominator, not a pick
        // weight: randSeedInt(weight) == 0 removes this destination.
        if (route->weight && rng.randSeedInt(route->weight) == 0) continue;
        eligible[eligibleCount++] = route;
    }
    if (!eligibleCount) return ClassicBiomeTransitionResult::NoEligibleRoute;

    if (mapModifierAvailable && eligibleCount > 1) {
        if (!selectedDestination) return ClassicBiomeTransitionResult::AwaitingMapChoice;
        for (size_t i = 0; i < eligibleCount; ++i) {
            if (sameBiomeId(eligible[i]->to, selectedDestination)) {
                outputBiomeId = eligible[i]->to;
                return ClassicBiomeTransitionResult::Ok;
            }
        }
        return ClassicBiomeTransitionResult::InvalidMapChoice;
    }
    if (selectedDestination) return ClassicBiomeTransitionResult::InvalidMapChoice;

    const int32_t selectedIndex = rng.pickIndex(static_cast<uint32_t>(eligibleCount));
    if (selectedIndex < 0 || static_cast<size_t>(selectedIndex) >= eligibleCount)
        return ClassicBiomeTransitionResult::NoEligibleRoute;
    outputBiomeId = eligible[selectedIndex]->to;
    return ClassicBiomeTransitionResult::Ok;
}

} // namespace Pokerogue3DS
