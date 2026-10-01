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
    OutOfBalls
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
    PokemonCaptureEvent& output)
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

    const double ballMultiplier = getPokeballCatchMultiplier(ballType);
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
    double rawRate = baseCatch * statusMultiplier;
    if (rawRate < 1.0) rawRate = 1.0;
    const uint32_t modifiedRate = static_cast<uint32_t>(std::round(rawRate));
    event.modifiedCatchRate = modifiedRate;

    // Shake probability formula from Gen 6 / upstream PokéRogue:
    // shakeProbability = round(65536 / pow(255 / modifiedCatchRate, 0.1875))
    uint32_t shakeProb = 65535;
    if (modifiedRate < 255) {
        const double ratio = 255.0 / static_cast<double>(modifiedRate);
        const double prob = 65536.0 / std::pow(ratio, 0.1875);
        shakeProb = static_cast<uint32_t>(std::round(prob));
        if (shakeProb > 65535) shakeProb = 65535;
    }
    event.shakeProbability = shakeProb;

    // Upstream 3 shake checks:
    uint8_t shakes = 0;
    bool caught = true;
    for (uint8_t i = 0; i < 3; ++i) {
        if (modifiedRate >= 255) {
            ++shakes;
            continue;
        }
        const uint32_t roll = rng.intInRange(0, 65535);
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
