// Generated from pinned canonical PokéRogue species catch rates.
#pragma once

#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <cstring>

namespace Pokerogue3DS {

// Pinned src/data/pokeball.ts, MAX_PER_TYPE_POKEBALLS (Classic rule, not engine capacity).
inline constexpr uint16_t kClassicPokeballLimit = 99;

enum class PokeballType : uint8_t {
    Pokeball = 0,
    GreatBall = 1,
    UltraBall = 2,
    RogueBall = 3,
    MasterBall = 4,
    LuxuryBall = 5
};

inline const PokerogueContent::PokeballRewardProfile* pokeballRewardProfile(const char* itemId) {
    if (!itemId) return nullptr;
    for (const auto& profile : PokerogueContent::kPokeballRewardProfiles)
        if (!std::strcmp(profile.itemId, itemId)) return &profile;
    return nullptr;
}
inline bool applyPokeballReward(const PokerogueContent::PokeballRewardProfile& profile,
    uint16_t* counts, size_t count) {
    if (!counts || !profile.ballSymbol || !profile.count) return false;
    PokeballType type{};
    if (!std::strcmp(profile.ballSymbol, "POKEBALL")) type = PokeballType::Pokeball;
    else if (!std::strcmp(profile.ballSymbol, "GREAT_BALL")) type = PokeballType::GreatBall;
    else if (!std::strcmp(profile.ballSymbol, "ULTRA_BALL")) type = PokeballType::UltraBall;
    else if (!std::strcmp(profile.ballSymbol, "ROGUE_BALL")) type = PokeballType::RogueBall;
    else if (!std::strcmp(profile.ballSymbol, "MASTER_BALL")) type = PokeballType::MasterBall;
    else return false;
    const size_t index = static_cast<size_t>(type);
    if (index >= count || counts[index] > kClassicPokeballLimit) return false;
    const uint32_t next = counts[index] + profile.count;
    counts[index] = static_cast<uint16_t>(next > kClassicPokeballLimit ? kClassicPokeballLimit : next);
    return true;
}

inline double getPokeballCatchMultiplier(PokeballType type) {
    switch (type) {
        case PokeballType::Pokeball: return 1.0;
        case PokeballType::GreatBall: return 1.5;
        case PokeballType::UltraBall: return 2.0;
        case PokeballType::RogueBall: return 3.0;
        case PokeballType::MasterBall: return -1.0;
        case PokeballType::LuxuryBall: return 1.0;
        default: return 1.0;
    }
}

inline const char* getPokeballName(PokeballType type) {
    switch (type) {
        case PokeballType::Pokeball: return "Poké Ball";
        case PokeballType::GreatBall: return "Great Ball";
        case PokeballType::UltraBall: return "Ultra Ball";
        case PokeballType::RogueBall: return "Rogue Ball";
        case PokeballType::MasterBall: return "Master Ball";
        case PokeballType::LuxuryBall: return "Luxury Ball";
        default: return "Poké Ball";
    }
}

// Generated from pinned canonical raw species records; no parallel dex ceiling.
inline uint8_t speciesCatchRate(uint16_t dex) {
    for (const auto& row : PokerogueContent::kSpeciesCatchProfiles)
        if (row.speciesDex == dex) return row.catchRate;
    return 0; // Missing records cannot masquerade as a default catch rate.
}

enum class CaptureBlocker : uint8_t {
    None = 0,
    TrainerBattle,
    MultipleEnemies,
    TargetFainted,
    BossShieldActive,
    FinalBossUncatchable,
    OutOfBalls,
    InvalidInput,
    MissingSpeciesData
};

struct PokemonCaptureEvent {
    bool caught = false;
    uint8_t shakeCount = 0;
    bool isCritical = false;
    uint32_t modifiedCatchRate = 0;
    uint32_t shakeProbability = 0;
    CaptureBlocker blocker = CaptureBlocker::None;
};

// CommandPhase.handleBallCommand: non-final Classic bosses can bypass shields
// with Master Ball, or when the target has Wonder Guard (canApply=false).
inline bool pokemonBossShieldCaptureBlocked(const PokemonBossState& boss,
    uint16_t abilityId, PokeballType ball, bool& output) {
    const auto* ability = PokerogueContent::findAbilityMovegenProfile(abilityId);
    if (!ability || !ability->sourceSymbol || (boss.segmentCount && boss.segmentIndex >= boss.segmentCount)) return false;
    const bool wonderGuard = std::strcmp(ability->sourceSymbol, "AbilityId.WONDER_GUARD") == 0;
    output = boss.segmentCount && boss.segmentIndex >= 1 &&
        ball != PokeballType::MasterBall && !wonderGuard;
    return true;
}

// Pinned getCriticalCaptureChance and CriticalCatchChanceBoosterModifier.
// caughtSpeciesCount must come from caughtAttr records, never candy/profile rows.
struct PokemonCaptureCriticalPolicy {
    bool resolved = false;
    bool freshStartChallenge = false;
    bool dailyMode = false;
    uint32_t caughtSpeciesCount = 0;
    uint8_t catchingCharmStacks = 0;
};
inline bool pokemonCriticalCaptureChance(uint32_t modifiedRate,
    const PokemonCaptureCriticalPolicy& policy, uint32_t& output) {
    if (!policy.resolved || policy.catchingCharmStacks > 3) return false;
    if (policy.freshStartChallenge) { output = 0; return true; }
    const double dexMultiplier = policy.dailyMode || policy.caughtSpeciesCount > 800 ? 2.5
        : policy.caughtSpeciesCount > 600 ? 2.0 : policy.caughtSpeciesCount > 400 ? 1.5
        : policy.caughtSpeciesCount > 200 ? 1.0 : policy.caughtSpeciesCount > 100 ? 0.5 : 0.0;
    const double charmMultiplier = policy.catchingCharmStacks ? 1.5 + policy.catchingCharmStacks / 2.0 : 1.0;
    output = static_cast<uint32_t>(std::floor(charmMultiplier * dexMultiplier *
        (modifiedRate > 255 ? 255 : modifiedRate) / 6.0));
    return true;
}

// Calculates modified catch rate matching upstream AttemptCapturePhase:
// modifiedCatchRate = round((((3*maxHp - 2*hp) * catchRate * pokeballMultiplier) / (3*maxHp)) * statusMultiplier)
// shakeProbability = round(65536 / ((255 / modifiedCatchRate) ^ 0.1875))
inline bool executeCaptureAttempt(
    const PokemonBattleState& target,
    PokeballType ballType,
    bool isTrainerBattle,
    bool multipleEnemiesAlive,
    bool isBossShieldActive,
    bool isWave200FinalBoss,
    PokerogueRngAdapter& rng,
    PokemonCaptureEvent& output,
    const PokemonCaptureCriticalPolicy* criticalPolicy = nullptr)
{
    PokemonCaptureEvent event{};
    if (isTrainerBattle) {
        event.blocker = CaptureBlocker::TrainerBattle;
        output = event;
        return false;
    }
    if (multipleEnemiesAlive) {
        event.blocker = CaptureBlocker::MultipleEnemies;
        output = event;
        return false;
    }
    if (target.hp == 0 || target.maxHp == 0) {
        event.blocker = CaptureBlocker::TargetFainted;
        output = event;
        return false;
    }
    if (isWave200FinalBoss) {
        event.blocker = CaptureBlocker::FinalBossUncatchable;
        output = event;
        return false;
    }
    if (isBossShieldActive && ballType != PokeballType::MasterBall) {
        event.blocker = CaptureBlocker::BossShieldActive;
        output = event;
        return false;
    }

    if (target.hp > target.maxHp || static_cast<uint8_t>(ballType) > static_cast<uint8_t>(PokeballType::LuxuryBall)) {
        event.blocker = CaptureBlocker::InvalidInput;
        output = event;
        return false;
    }
    bool catchProfileFound = false;
    for (const auto& row : PokerogueContent::kSpeciesCatchProfiles)
        catchProfileFound |= row.speciesDex == target.speciesDex;
    if (!catchProfileFound) {
        event.blocker = CaptureBlocker::MissingSpeciesData;
        output = event;
        return false;
    }
    if (criticalPolicy && (!criticalPolicy->resolved || criticalPolicy->catchingCharmStacks > 3)) {
        event.blocker = CaptureBlocker::InvalidInput;
        output = event;
        return false;
    }
    const double ballMultiplier = getPokeballCatchMultiplier(ballType);
    // AttemptCapturePhase.start always requests randBattleSeedInt(256),
    // including a zero critical chance and guaranteed Master Ball. Current
    // host must provide a resolved caught-dex/charm policy for nonzero
    // probability; the legacy diagnostic path does not infer one from candy data.
    const uint32_t criticalRoll = static_cast<uint32_t>(rng.randSeedInt(256));
    if (ballMultiplier < 0.0) {
        // Master Ball guarantees capture immediately.
        event.caught = true;
        event.shakeCount = 3;
        event.modifiedCatchRate = 255;
        event.shakeProbability = 65535;
        output = event;
        return true;
    }

    const uint8_t catchRate = speciesCatchRate(target.speciesDex);
    const double threeMax = 3.0 * target.maxHp;
    const double twoHp = 2.0 * target.hp;
    const double baseCatch = ((threeMax - twoHp) * catchRate * ballMultiplier) / threeMax;
    // Status multiplier: neutral = 1.0 (expandable when volatile/non-volatile statuses apply).
    const double statusMultiplier = 1.0;
    const double rawRate = baseCatch * statusMultiplier;
    const uint32_t modifiedRate = static_cast<uint32_t>(std::round(rawRate));
    event.modifiedCatchRate = modifiedRate;

    // Shake probability formula from Gen 6 / upstream PokéRogue:
    // shakeProbability = round(65536 / pow(255 / modifiedCatchRate, 0.1875))
    // Zero rate has zero probability (upstream division yields Infinity).
    // Keep the formula's reported value above 65535 for guaranteed catches;
    // the >=255 branch below skips the actual shake RNG checks.
    const uint32_t shakeProb = modifiedRate ? static_cast<uint32_t>(std::round(
        65536.0 / std::pow(255.0 / static_cast<double>(modifiedRate), 0.1875))) : 0;
    event.shakeProbability = shakeProb;
    uint32_t criticalChance = 0;
    if (criticalPolicy && !pokemonCriticalCaptureChance(modifiedRate, *criticalPolicy, criticalChance)) return false;
    event.isCritical = criticalRoll < criticalChance;
    if (event.isCritical) {
        // Upstream skips the visual shake's RNG, then performs one check.
        event.shakeCount = 1;
        event.caught = static_cast<uint32_t>(rng.randSeedInt(65536)) < shakeProb;
        output = event;
        return true;
    }


    // Upstream 3 shake checks:
    uint8_t shakes = 0;
    bool caught = true;
    for (uint8_t i = 0; i < 3; ++i) {
        if (modifiedRate >= 255) {
            ++shakes;
            continue;
        }
        const uint32_t roll = rng.randSeedInt(65536);
        if (roll < shakeProb) {
            ++shakes;
        } else {
            caught = false;
            break;
        }
    }

    event.shakeCount = shakes;
    event.caught = caught;
    output = event;
    return true;
}

} // namespace Pokerogue3DS
