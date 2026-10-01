#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokemonBattleState.hpp"
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

struct PokemonFriendshipPolicy {
    bool resolved = false; // Includes timed events, fusion and held booster applicability.
    uint8_t boosterStacks = 0;
    bool capped = false;
    uint8_t friendshipCap = 0; // Caller resolves the pinned Rare Candy cap.
    double candyMultiplier = 1.0;
};

struct PokemonFriendshipChangePlan {
    uint8_t friendship = 0;
    uint32_t candyFriendshipGain = 0; // Each resolved root species receives this gain.
    bool requiresMaxFriendshipCallbacks = false;
};

// Pinned Pokemon.addFriendship / PokemonFriendshipBoosterModifier.apply.
// Does not publish an actor change before the starter ledger/callbacks are ready.
inline PokemonExperienceResult planPokemonFriendshipChange(uint8_t currentFriendship,
    int32_t gain, const PokemonFriendshipPolicy& policy, PokemonFriendshipChangePlan& output) {
    PokemonFriendshipChangePlan next{};
    if (gain <= 0) {
        const int64_t value = static_cast<int64_t>(currentFriendship) + gain;
        next.friendship = static_cast<uint8_t>(value > 0 ? value : 0);
        output = next;
        return PokemonExperienceResult::Ok;
    }
    if (!policy.resolved) return PokemonExperienceResult::UnresolvedPolicy;
    if (policy.boosterStacks > 3 || !(policy.candyMultiplier >= 0.0))
        return PokemonExperienceResult::UnresolvedPolicy;
    const double boosted = gain * (1.0 + 0.5 * policy.boosterStacks);
    if (!(boosted <= 4294967295.0)) return PokemonExperienceResult::Overflow;
    const uint32_t boostedGain = static_cast<uint32_t>(boosted);
    uint64_t friendship = static_cast<uint64_t>(currentFriendship) + boostedGain;
    if (policy.capped && friendship > policy.friendshipCap)
        friendship = currentFriendship > policy.friendshipCap ? currentFriendship : policy.friendshipCap;
    next.friendship = static_cast<uint8_t>(friendship > 255 ? 255 : friendship);
    next.requiresMaxFriendshipCallbacks = next.friendship == 255;
    const double candy = boostedGain * policy.candyMultiplier;
    if (!(candy >= 0.0 && candy <= 4294967295.0)) return PokemonExperienceResult::Overflow;
    next.candyFriendshipGain = static_cast<uint32_t>(candy);
    output = next;
    return PokemonExperienceResult::Ok;
}

// Called once by the resolved player faint phase, after instant-revive checks.
inline bool applyPokemonFaintFriendship(PokemonBattleState& actor) {
    if (actor.hp) return false;
    PokemonFriendshipChangePlan plan{};
    PokemonFriendshipPolicy unused{};
    if (planPokemonFriendshipChange(actor.friendship,
            -static_cast<int32_t>(PokerogueContent::kFriendshipLossFromFaint), unused, plan) !=
            PokemonExperienceResult::Ok) return false;
    actor.friendship = plan.friendship;
    return true;
}

struct StarterCandyProgressPlan {
    uint32_t friendship = 0;
    uint32_t candyAward = 0;
};

// Pinned starterData update: rejected candy awards leave progress at cap - 1.
// A fused actor must apply this independently to both resolved root species.
inline PokemonExperienceResult planStarterCandyProgress(uint32_t currentFriendship,
    uint32_t gain, uint32_t friendshipCap, bool awardPolicyResolved, bool awardAccepted,
    StarterCandyProgressPlan& output) {
    if (!friendshipCap) return PokemonExperienceResult::InvalidLevel;
    const uint64_t total = static_cast<uint64_t>(currentFriendship) + gain;
    if (total > 0xffffffffULL) return PokemonExperienceResult::Overflow;
    StarterCandyProgressPlan next{};
    next.friendship = static_cast<uint32_t>(total);
    if (total >= friendshipCap) {
        if (!awardPolicyResolved) return PokemonExperienceResult::UnresolvedPolicy;
        if (awardAccepted) {
            next.candyAward = static_cast<uint32_t>(total / friendshipCap);
            next.friendship = static_cast<uint32_t>(total % friendshipCap);
        } else next.friendship = friendshipCap - 1;
    }
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

// Current wild-double frontier forbids capture while both enemies are alive.
// VictoryPhase applies friendship once per defeated enemy; capture does not.
// This count does not replace the future per-faint EXP/phase queue.
inline uint8_t pokemonVictoryFriendshipDefeats(bool doubleBattle, bool finalEnemyDefeated) {
    return static_cast<uint8_t>((doubleBattle ? 1 : 0) + (finalEnemyDefeated ? 1 : 0));
}

inline uint8_t pokemonPendingDoubleExperienceMask(bool primaryFainted, bool secondaryFainted, uint8_t granted) {
    const uint8_t fainted = static_cast<uint8_t>((primaryFainted ? 1 : 0) | (secondaryFainted ? 2 : 0));
    return static_cast<uint8_t>(fainted & ~granted & 3);
}

struct PokemonParticipantExperiencePolicy {
    bool resolved = false; // Includes recipient eligibility and all per-member modifiers.
    uint8_t participantCount = 0;
    bool participated = false;
    bool eligible = false; // Living and strictly below the resolved EXP level cap.
    uint8_t expShareStacks = 0;
    uint8_t multipleParticipantBonusStacks = 0;
    bool pokerus = false;
    double boosterMultiplier = 1.0;
    bool hasMultiplierOverride = false;
    double multiplierOverride = 0.0;
};

// Per-member applyPartyExp allocation before the separate ExpBalance pass.
// Trainer EXP is floored BEFORE sharing, then the member award is floored after
// Pokerus/override/held booster. No friendship is derived from this XP amount.
inline PokemonExperienceResult pokemonParticipantExperience(double defeatExperience,
    bool trainerBattle, const PokemonParticipantExperiencePolicy& policy, uint32_t& output) {
    if (!policy.resolved) return PokemonExperienceResult::UnresolvedPolicy;
    if (policy.participantCount > 6) return PokemonExperienceResult::InvalidLevel;
    if (!(defeatExperience >= 0.0 && defeatExperience <= 4294967295.0) ||
        !(policy.boosterMultiplier >= 0.0 && policy.boosterMultiplier <= 4294967295.0) ||
        (policy.hasMultiplierOverride && !(policy.multiplierOverride >= 0.0 &&
            policy.multiplierOverride <= 4294967295.0))) return PokemonExperienceResult::Overflow;
    if (!policy.eligible || !policy.participantCount ||
        (!policy.participated && !policy.expShareStacks)) {
        output = 0;
        return PokemonExperienceResult::Ok;
    }
    double experience = defeatExperience;
    if (trainerBattle) {
        const double boosted = experience * 1.5;
        if (!(boosted <= 4294967295.0)) return PokemonExperienceResult::Overflow;
        experience = static_cast<uint32_t>(boosted);
    }
    double multiplier = policy.participated ? 1.0 / policy.participantCount
        : (policy.expShareStacks * 0.2) / policy.participantCount;
    if (policy.participated && policy.participantCount > 1)
        multiplier += policy.multipleParticipantBonusStacks * 0.2;
    if (policy.pokerus) multiplier *= 1.5;
    if (policy.hasMultiplierOverride) multiplier = policy.multiplierOverride;
    const double award = experience * multiplier * policy.boosterMultiplier;
    if (!(award >= 0.0 && award <= 4294967295.0)) return PokemonExperienceResult::Overflow;
    output = static_cast<uint32_t>(award);
    return PokemonExperienceResult::Ok;
}

// Pinned BattleScene.applyPartyExp: one living participant, no EXP modifiers.
// Trainer multiplication is floored before participant distribution; wild EXP
// is floored at the final per-member award. Multi-member sharing is separate.
inline PokemonExperienceResult pokemonSingleParticipantExperience(
    double defeatExperience, bool trainerBattle, uint32_t& output) {
    output = 0;
    PokemonParticipantExperiencePolicy policy{};
    policy.resolved = true;
    policy.participantCount = 1;
    policy.participated = true;
    policy.eligible = true;
    return pokemonParticipantExperience(defeatExperience, trainerBattle, policy, output);
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
