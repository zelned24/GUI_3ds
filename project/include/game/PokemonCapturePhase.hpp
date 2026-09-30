// Generated from pinned canonical PokéRogue species catch rates.
#pragma once

#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace Pokerogue3DS {

enum class PokeballType : uint8_t {
    Pokeball = 0,
    GreatBall = 1,
    UltraBall = 2,
    RogueBall = 3,
    MasterBall = 4,
    LuxuryBall = 5
};

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

// Pinned canonical species catch rates from generation data.
inline constexpr uint8_t kSpeciesCatchRates[1026] = {
    0, 45, 45, 45, 45, 45, 45, 45, 45, 45, 255, 120, 45, 255, 120, 45, 255, 120, 45, 255, 127, 255, 90, 255, 90,
    190, 75, 255, 90, 235, 120, 45, 235, 120, 45, 150, 25, 190, 75, 170, 50, 255, 90, 255, 120, 45, 190, 75, 190, 75,
    255, 50, 255, 90, 190, 75, 190, 75, 190, 75, 255, 120, 45, 200, 100, 50, 180, 90, 45, 255, 120, 45, 190, 60, 255,
    120, 45, 190, 60, 190, 75, 190, 60, 45, 190, 45, 190, 75, 190, 75, 190, 60, 190, 90, 45, 45, 190, 75, 225, 60,
    190, 60, 90, 45, 190, 75, 45, 45, 45, 190, 60, 120, 60, 30, 45, 45, 225, 75, 225, 60, 225, 60, 45, 45, 45,
    45, 45, 45, 45, 255, 45, 45, 35, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 25, 3, 3, 3, 45, 45, 45,
    3, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 255, 90, 255, 90, 255, 90, 255, 90, 90, 190, 75, 190, 150, 170,
    190, 75, 190, 75, 235, 120, 45, 45, 190, 75, 65, 45, 255, 120, 45, 45, 235, 120, 75, 255, 90, 45, 45, 30, 70,
    45, 225, 45, 60, 190, 75, 190, 60, 25, 190, 75, 45, 25, 190, 45, 60, 120, 60, 190, 75, 225, 75, 60, 190, 75,
    45, 25, 25, 120, 45, 45, 120, 60, 45, 45, 45, 75, 45, 45, 45, 45, 45, 30, 3, 3, 3, 45, 45, 45, 3,
    3, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 255, 127, 255, 90, 255, 120, 45, 120, 45, 255, 120, 45, 255, 120,
    45, 200, 45, 190, 45, 235, 120, 45, 200, 75, 255, 90, 255, 120, 45, 255, 120, 45, 190, 120, 45, 180, 200, 150, 255,
    255, 60, 45, 45, 180, 90, 45, 180, 90, 120, 45, 200, 200, 150, 150, 150, 225, 75, 225, 60, 125, 60, 255, 150, 90,
    255, 60, 255, 255, 120, 45, 190, 60, 255, 45, 90, 90, 45, 45, 190, 75, 205, 155, 255, 90, 45, 45, 45, 45, 255,
    60, 45, 200, 225, 45, 190, 90, 200, 45, 30, 125, 190, 75, 255, 120, 45, 255, 60, 60, 25, 225, 45, 45, 45, 45,
    25, 10, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 45, 45, 45, 45, 45, 45, 45, 45, 45, 255, 120, 45, 255,
    127, 255, 45, 235, 120, 45, 255, 75, 45, 45, 45, 45, 120, 45, 45, 120, 45, 200, 190, 75, 190, 75, 190, 75, 45,
    125, 60, 190, 60, 45, 30, 190, 75, 120, 225, 60, 255, 90, 255, 145, 130, 30, 100, 45, 45, 45, 50, 75, 45, 140,
    60, 120, 45, 140, 75, 200, 190, 75, 25, 120, 60, 45, 30, 30, 30, 30, 30, 30, 30, 30, 45, 45, 30, 50, 30,
    45, 60, 45, 75, 45, 3, 3, 3, 3, 3, 3, 3, 3, 3, 30, 3, 3, 45, 3, 3, 45, 45, 45, 45, 45,
    45, 45, 45, 45, 255, 255, 255, 120, 45, 255, 90, 190, 75, 190, 75, 190, 75, 190, 75, 255, 120, 45, 190, 75, 255,
    120, 45, 190, 45, 120, 60, 255, 180, 90, 45, 255, 120, 45, 45, 45, 255, 120, 45, 255, 120, 45, 190, 75, 190, 75,
    190, 180, 90, 45, 120, 60, 255, 190, 75, 180, 90, 45, 190, 90, 45, 45, 45, 45, 190, 60, 75, 45, 255, 60, 200,
    100, 50, 200, 100, 50, 190, 45, 255, 120, 45, 190, 75, 200, 200, 75, 190, 75, 190, 60, 75, 190, 75, 255, 90, 130,
    60, 30, 190, 60, 30, 255, 90, 190, 90, 45, 75, 60, 45, 120, 60, 25, 200, 75, 75, 180, 45, 45, 190, 90, 120,
    45, 45, 190, 60, 190, 60, 90, 90, 45, 45, 45, 45, 15, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    45, 45, 45, 45, 45, 45, 45, 45, 45, 255, 127, 255, 120, 45, 255, 120, 45, 220, 65, 225, 120, 45, 200, 45, 220,
    65, 160, 190, 75, 180, 90, 45, 200, 140, 200, 140, 190, 80, 120, 45, 225, 55, 225, 55, 190, 75, 45, 45, 45, 45,
    45, 100, 180, 60, 45, 45, 45, 75, 120, 60, 120, 60, 190, 55, 190, 45, 45, 45, 3, 3, 3, 3, 45, 45, 45,
    45, 45, 45, 45, 45, 45, 255, 120, 45, 255, 127, 255, 120, 45, 225, 60, 45, 190, 75, 190, 90, 60, 190, 75, 190,
    60, 200, 100, 190, 75, 190, 75, 120, 45, 140, 70, 235, 120, 45, 60, 45, 45, 90, 45, 140, 60, 60, 3, 3, 30,
    45, 70, 180, 45, 80, 70, 25, 45, 45, 45, 3, 3, 3, 3, 3, 3, 3, 3, 45, 45, 45, 45, 45, 45, 45,
    3, 3, 3, 45, 45, 30, 30, 3, 3, 3, 45, 45, 45, 45, 45, 45, 45, 45, 45, 255, 90, 255, 120, 45, 255,
    120, 45, 255, 127, 190, 75, 255, 127, 255, 75, 255, 45, 255, 120, 45, 255, 45, 45, 255, 120, 45, 255, 60, 75, 45,
    190, 75, 180, 45, 120, 60, 235, 120, 45, 255, 120, 45, 45, 90, 30, 45, 45, 90, 200, 100, 45, 75, 190, 75, 60,
    60, 30, 180, 190, 90, 45, 45, 45, 45, 45, 45, 45, 45, 10, 10, 45, 3, 3, 3, 3, 3, 3, 3, 3, 45,
    15, 20, 45, 20, 45, 3, 45, 45, 45, 45, 45, 45, 45, 45, 45, 255, 100, 255, 120, 190, 30, 190, 80, 45, 150,
    75, 190, 90, 255, 120, 45, 190, 255, 120, 45, 90, 25, 25, 190, 50, 180, 90, 150, 75, 190, 90, 190, 45, 190, 90,
    120, 190, 75, 190, 45, 120, 60, 190, 90, 45, 255, 50, 25, 200, 45, 190, 75, 190, 25, 70, 25, 120, 60, 100, 150,
    50, 100, 25, 100, 45, 90, 45, 45, 25, 30, 50, 50, 30, 30, 30, 30, 50, 50, 30, 30, 30, 45, 25, 10, 45,
    45, 6, 6, 6, 6, 10, 10, 3, 3, 10, 10, 45, 120, 60, 3, 3, 3, 5, 10, 10, 10, 10, 10, 10, 5,
    3
};

inline uint8_t speciesCatchRate(uint16_t dex) {
    if (dex <= 1025) return kSpeciesCatchRates[dex];
    switch (dex) {
        case 2019: return 255; // alola_rattata
        case 2020: return 127; // alola_raticate
        case 2026: return 75; // alola_raichu
        case 2027: return 255; // alola_sandshrew
        case 2028: return 90; // alola_sandslash
        case 2037: return 190; // alola_vulpix
        case 2038: return 75; // alola_ninetales
        case 2050: return 255; // alola_diglett
        case 2051: return 50; // alola_dugtrio
        case 2052: return 255; // alola_meowth
        case 2053: return 90; // alola_persian
        case 2074: return 255; // alola_geodude
        case 2075: return 120; // alola_graveler
        case 2076: return 45; // alola_golem
        case 2088: return 190; // alola_grimer
        case 2089: return 75; // alola_muk
        case 2103: return 45; // alola_exeggutor
        case 2105: return 75; // alola_marowak
        case 2658: return 45; // battle_bond_greninja
        case 2670: return 120; // eternal_floette
        case 4052: return 255; // galar_meowth
        case 4077: return 190; // galar_ponyta
        case 4078: return 60; // galar_rapidash
        case 4079: return 190; // galar_slowpoke
        case 4080: return 75; // galar_slowbro
        case 4083: return 45; // galar_farfetchd
        case 4110: return 60; // galar_weezing
        case 4122: return 45; // galar_mr_mime
        case 4144: return 3; // galar_articuno
        case 4145: return 3; // galar_zapdos
        case 4146: return 3; // galar_moltres
        case 4199: return 70; // galar_slowking
        case 4222: return 60; // galar_corsola
        case 4263: return 255; // galar_zigzagoon
        case 4264: return 90; // galar_linoone
        case 4554: return 120; // galar_darumaka
        case 4555: return 60; // galar_darmanitan
        case 4562: return 190; // galar_yamask
        case 4618: return 75; // galar_stunfisk
        case 6058: return 190; // hisui_growlithe
        case 6059: return 85; // hisui_arcanine
        case 6100: return 190; // hisui_voltorb
        case 6101: return 60; // hisui_electrode
        case 6157: return 45; // hisui_typhlosion
        case 6211: return 45; // hisui_qwilfish
        case 6215: return 60; // hisui_sneasel
        case 6503: return 45; // hisui_samurott
        case 6549: return 75; // hisui_lilligant
        case 6550: return 190; // hisui_basculin
        case 6570: return 75; // hisui_zorua
        case 6571: return 45; // hisui_zoroark
        case 6628: return 60; // hisui_braviary
        case 6705: return 45; // hisui_sliggoo
        case 6706: return 45; // hisui_goodra
        case 6713: return 55; // hisui_avalugg
        case 6724: return 45; // hisui_decidueye
        case 8128: return 45; // paldea_tauros
        case 8194: return 255; // paldea_wooper
        case 8901: return 75; // bloodmoon_ursaluna
        default: return 45;
    }
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
    if (isBossShieldActive) {
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
