#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include <cstdint>
#include <cstring>

namespace Pokerogue3DS {

enum class PokemonExperienceResult : uint8_t {
    Ok = 0,
    InvalidLevel,
    UnknownGrowthRate,
    Overflow,
    UnresolvedPolicy,
};

struct PokemonExperienceProgress {
    uint16_t level = 0;
    uint32_t totalExperience = 0;
};

inline bool pokemonGrowthRateIndex(const char* name, uint8_t& output) {
    if (!name) return false;
    static constexpr const char* names[] = {
        "ERRATIC", "FAST", "MEDIUM_FAST", "MEDIUM_SLOW", "SLOW", "FLUCTUATING"
    };
    for (uint8_t i = 0; i < 6; ++i) {
        const char* left = name;
        const char* right = names[i];
        while (*left && *left == *right) { ++left; ++right; }
        if (*left == *right) { output = i; return true; }
    }
    return false;
}

inline PokemonExperienceResult pokemonTotalExperienceForLevel(
    const char* growthRate, uint16_t level, uint32_t& output) {
    output = 0;
    uint8_t rate = 0;
    if (!pokemonGrowthRateIndex(growthRate, rate)) return PokemonExperienceResult::UnknownGrowthRate;
    if (!level || level > 10000) return PokemonExperienceResult::InvalidLevel;

    if (level < 100) {
        const uint32_t value = PokerogueContent::kExperienceLevels[rate][level - 1];
        const uint32_t mediumFast = PokerogueContent::kExperienceLevels[2][level - 1];
        const double result = rate == 2 ? value : value * 0.325 + mediumFast * 0.675;
        if (result < 0.0 || result > 4294967295.0) return PokemonExperienceResult::Overflow;
        output = static_cast<uint32_t>(result); // upstream Math.floor for non-MEDIUM_FAST curves
        return PokemonExperienceResult::Ok;
    }

    const double n = level;
    const double n2 = n * n;
    const double n3 = n2 * n;
    double raw = 0.0;
    switch (rate) {
    case 0: raw = (n2 * n2 + n3 * 2000.0) / 3500.0; break;
    case 1: raw = n3 * 4.0 / 5.0; break;
    case 2: raw = n3; break;
    case 3: raw = n3 * 6.0 / 5.0 - 15.0 * n2 + 100.0 * n - 140.0; break;
    case 4: raw = n3 * 5.0 / 4.0; break;
    case 5: raw = n3 * (n / 2.0 + 8.0) * 4.0 / (100.0 + n); break;
    default: return PokemonExperienceResult::UnknownGrowthRate;
    }
    const double result = rate == 2 ? raw : raw * 0.325 + n3 * 0.675;
    if (result < 0.0 || result > 4294967295.0) return PokemonExperienceResult::Overflow;
    output = static_cast<uint32_t>(result); // upstream Math.floor
    return PokemonExperienceResult::Ok;
}

inline const PokerogueContent::LevelIncrementItemProfile* levelIncrementItemProfile(const char* itemId) {
    if (!itemId) return nullptr;
    for (const auto& profile : PokerogueContent::kLevelIncrementItemProfiles)
        if (std::strcmp(profile.itemId, itemId) == 0) return &profile;
    return nullptr;
}

struct PokemonLevelIncrementPlan {
    PokemonExperienceProgress progress{};
    uint16_t previousLevel = 0;
    bool requiresFriendship = true;
    bool requiresLevelUpPhase = true;
};

// Numeric part of pinned PokemonLevelIncrementModifier.apply. This is a plan,
// not item consumption: friendship/candy progress and LevelUpPhase must follow.
inline PokemonExperienceResult planPokemonLevelIncrement(
    const char* growthRate, uint16_t currentLevel, uint32_t currentExperience,
    uint16_t candyJarStacks, bool boosterPolicyResolved, uint16_t uncappedExpLimit,
    PokemonLevelIncrementPlan& output) {
    if (!boosterPolicyResolved) return PokemonExperienceResult::UnresolvedPolicy;
    if (candyJarStacks > 99 || !currentLevel || !uncappedExpLimit)
        return PokemonExperienceResult::InvalidLevel;
    const uint32_t level = static_cast<uint32_t>(currentLevel) + 1 + candyJarStacks;
    if (level > 10000) return PokemonExperienceResult::InvalidLevel;
    uint8_t rate = 0;
    if (!pokemonGrowthRateIndex(growthRate, rate)) return PokemonExperienceResult::UnknownGrowthRate;
    uint32_t experience = currentExperience;
    if (level <= uncappedExpLimit) {
        const auto status = pokemonTotalExperienceForLevel(growthRate, static_cast<uint16_t>(level), experience);
        if (status != PokemonExperienceResult::Ok) return status;
    }
    PokemonLevelIncrementPlan next{};
    next.progress = {static_cast<uint16_t>(level), experience};
    next.previousLevel = currentLevel;
    output = next;
    return PokemonExperienceResult::Ok;
}

inline PokemonExperienceResult pokemonExperienceForDefeat(
    const PokerogueContent::Species& defeated, uint16_t defeatedLevel, double& output,
    const PokerogueContent::Form* defeatedForm = nullptr) {
    output = 0.0;
    if (!defeatedLevel || defeatedLevel > 10000) return PokemonExperienceResult::InvalidLevel;
    // Pinned Pokemon.getExpValue(): (getSpeciesForm().getBaseExp() * level) / 5 + 1.
    // PokemonSpeciesForm.getBaseExp multiplies these form keys by 1.5.
    double baseExp = defeated.baseExp;
    if (defeatedForm) {
        const char* key = defeatedForm->formKey;
        if (key && (
            std::strcmp(key, "MEGA") == 0 || std::strcmp(key, "MEGA_X") == 0 ||
            std::strcmp(key, "MEGA_Y") == 0 || std::strcmp(key, "MEGA_Z") == 0 ||
            std::strcmp(key, "MEGA_ORIGINAL") == 0 || std::strcmp(key, "MEGA_CURLY") == 0 ||
            std::strcmp(key, "MEGA_DROOPY") == 0 || std::strcmp(key, "MEGA_STRETCHY") == 0 ||
            std::strcmp(key, "PRIMAL") == 0 || std::strcmp(key, "GIGANTAMAX") == 0 ||
            std::strcmp(key, "ETERNAMAX") == 0)) {
            baseExp *= 1.5;
        }
    }
    output = baseExp * defeatedLevel / 5.0 + 1.0;
    return PokemonExperienceResult::Ok;
}

// Pinned BattleScene.applyPartyExp: one living participant, no EXP modifiers.
// Trainer multiplication is floored before participant distribution; wild EXP
// is floored at the final per-member award. Multi-member sharing is separate.
inline PokemonExperienceResult pokemonSingleParticipantExperience(
    double defeatExperience, bool trainerBattle, uint32_t& output) {
    output = 0;
    if (!(defeatExperience >= 0.0)) return PokemonExperienceResult::Overflow;
    const double award = defeatExperience * (trainerBattle ? 1.5 : 1.0);
    if (!(award <= 4294967295.0)) return PokemonExperienceResult::Overflow;
    output = static_cast<uint32_t>(award);
    return PokemonExperienceResult::Ok;
}

inline uint16_t classicExperienceLevelCap(uint16_t waveIndex) {
    if (!waveIndex || waveIndex > 200) return 0;
    const uint16_t difficultyWave = static_cast<uint16_t>(((waveIndex + 9) / 10) * 10);
    const double wave = difficultyWave;
    const double baseLevel = (1.0 + wave / 2.0 + (wave / 25.0) * (wave / 25.0)) * 1.2;
    // Pinned BattleScene.getMaxExpLevel rounds upward to the next even level,
    // then adds two levels of headroom.
    const uint16_t evenCeiling = static_cast<uint16_t>(2.0 * ((baseLevel / 2.0) == static_cast<uint16_t>(baseLevel / 2.0)
        ? static_cast<uint16_t>(baseLevel / 2.0) : static_cast<uint16_t>(baseLevel / 2.0) + 1));
    return static_cast<uint16_t>(evenCeiling + 2);
}

// Mirrors Pokemon.addExp(exp): levels advance against the pinned growth curve,
// and XP gained past the active mode cap is clipped without discarding XP held
// before that cap was reached.
inline PokemonExperienceResult applyPokemonExperience(
    const char* growthRate,
    uint16_t currentLevel,
    uint32_t currentTotalExperience,
    uint32_t gainedExperience,
    uint16_t levelCap,
    PokemonExperienceProgress& output) {
    output = {};
    uint8_t ignoredRate = 0;
    if (!pokemonGrowthRateIndex(growthRate, ignoredRate)) return PokemonExperienceResult::UnknownGrowthRate;
    if (!currentLevel || currentLevel > 10000 || !levelCap || levelCap > 10000)
        return PokemonExperienceResult::InvalidLevel;

    auto status = PokemonExperienceResult::Ok;

    const uint64_t gainedTotal = static_cast<uint64_t>(currentTotalExperience) + gainedExperience;
    if (gainedTotal > 0xFFFFFFFFull) return PokemonExperienceResult::Overflow;
    uint32_t total = static_cast<uint32_t>(gainedTotal);
    uint16_t level = currentLevel;
    while (level < levelCap) {
        uint32_t nextThreshold = 0;
        status = pokemonTotalExperienceForLevel(growthRate, static_cast<uint16_t>(level + 1), nextThreshold);
        if (status != PokemonExperienceResult::Ok) return status;
        if (total < nextThreshold) break;
        ++level;
    }

    if (level >= levelCap) {
        uint32_t capThreshold = 0;
        status = pokemonTotalExperienceForLevel(growthRate, level, capThreshold);
        if (status != PokemonExperienceResult::Ok) return status;
        total = capThreshold > currentTotalExperience ? capThreshold : currentTotalExperience;
    }
    output = {level, total};
    return PokemonExperienceResult::Ok;
}

} // namespace Pokerogue3DS
