#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokerogueRngAdapter.hpp"
#include <algorithm>
#include <cstdint>

namespace Pokerogue3DS {

enum class ClassicWaveKind : uint8_t {
    Invalid = 0,
    RegularWild,
    TrainerChanceRequired,
    FixedTrainerBattle,
    MajorBoss,
    FinalBoss,
};

enum class ClassicTrainerDecision : uint8_t { Invalid = 0, NoTrainer, Trainer };

// Classifies only source-declared Classic schedule facts. Random trainer
// selection still requires the arena trainerChance seed-offset algorithm.
inline ClassicWaveKind classifyClassicWave(uint16_t wave) {
    if (!wave || wave > PokerogueContent::kClassicFinalWave) return ClassicWaveKind::Invalid;
    if (wave == PokerogueContent::kClassicFinalWave) return ClassicWaveKind::FinalBoss;
    if (PokerogueContent::findClassicFixedBattleWave(wave))
        return ClassicWaveKind::FixedTrainerBattle;
    if (wave % 30 == 20) return ClassicWaveKind::FixedTrainerBattle;
    if (PokerogueContent::isClassicMajorBossWave(wave)) return ClassicWaveKind::MajorBoss;
    // GameMode.isWaveTrainer does not roll generic trainer chance on X1 floors.
    if (wave % 10 == 1) return ClassicWaveKind::RegularWild;
    return ClassicWaveKind::TrainerChanceRequired;
}

// Mirrors the pinned Classic branch of GameMode.isWaveTrainer for ordinary
// trainer-chance floors. The caller supplies the live upstream RNG stream at
// the exact call point; nearby past waves use independent seed-offset streams.
// This only resolves scheduling, never a Trainer record/team or battle.
inline ClassicTrainerDecision resolveClassicTrainerChance(
    uint16_t wave, const char* biomeId, bool offsetGym,
    const uint16_t* rootSeed, size_t seedLength,
    PokerogueRngAdapter& currentWaveRng, bool& shouldSpawn) {
    shouldSpawn = false;
    if (!wave || wave >= PokerogueContent::kClassicFinalWave || !biomeId || !*biomeId ||
        seedLength > PokerogueRngAdapter::kMaxSeedCodeUnits || (seedLength && !rootSeed))
        return ClassicTrainerDecision::Invalid;
    if (PokerogueContent::findClassicFixedBattleWave(wave)) return ClassicTrainerDecision::Invalid;
    const uint16_t gymRemainder = offsetGym ? 0 : 20;
    if (wave % 30 == gymRemainder) {
        shouldSpawn = true;
        return ClassicTrainerDecision::Trainer;
    }
    const uint16_t waveRemainder = wave % 10;
    if (waveRemainder == 0 || waveRemainder == 1) return ClassicTrainerDecision::NoTrainer;

    const auto* biomeChance = PokerogueContent::findBiomeTrainerChance(biomeId);
    if (!biomeChance) return ClassicTrainerDecision::Invalid;
    const int32_t trainerChance = biomeChance->denominator;
    if (!trainerChance) return ClassicTrainerDecision::NoTrainer;

    bool allowTrainerBattle = true;
    const uint16_t waveBase = static_cast<uint16_t>((wave / 10) * 10);
    const uint16_t first = std::max<uint16_t>(static_cast<uint16_t>(wave - 2), static_cast<uint16_t>(waveBase + 2));
    const uint16_t last = std::min<uint16_t>(static_cast<uint16_t>(wave + 2), static_cast<uint16_t>(waveBase + 10));
    for (uint16_t neighbor = first; neighbor <= last; ++neighbor) {
        if (neighbor == wave) continue;
        if (neighbor % 30 == gymRemainder || PokerogueContent::findClassicFixedBattleWave(neighbor)) {
            allowTrainerBattle = false;
            break;
        }
        if (neighbor < wave) {
            PokerogueRngAdapter priorWaveRng;
            PokerogueSeedOffsetScope offsetScope(priorWaveRng, rootSeed, seedLength, neighbor);
            if (!offsetScope.valid()) return ClassicTrainerDecision::Invalid;
            if (!priorWaveRng.randSeedInt(trainerChance)) {
                allowTrainerBattle = false;
                break;
            }
        }
    }
    if (allowTrainerBattle && currentWaveRng.randSeedInt(trainerChance) == 0) shouldSpawn = true;
    return shouldSpawn ? ClassicTrainerDecision::Trainer : ClassicTrainerDecision::NoTrainer;
}

} // namespace Pokerogue3DS
