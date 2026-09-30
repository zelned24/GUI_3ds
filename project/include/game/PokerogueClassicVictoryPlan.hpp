#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include <cstdint>

namespace Pokerogue3DS {

enum class ClassicVictoryStep : uint8_t {
    Experience,
    BattleEnd,
    EggLapse,
    LockCapsuleReward,
    TimedEventRewards,
    SelectModifier,
    FixedModifierRewards,
    SelectBiome,
    NewBattle,
    GameClear,
};

struct ClassicVictoryPlan {
    ClassicVictoryStep steps[9]{};
    uint8_t count = 0;
    uint16_t completedWave = 0;
    uint16_t nextWave = 0;

    bool contains(ClassicVictoryStep step) const {
        for (uint8_t i = 0; i < count; ++i) if (steps[i] == step) return true;
        return false;
    }
};

// Pinned source: src/phases/victory-phase.ts, VictoryPhase.start; and
// src/battle-scene.ts, BattleScene.isNewBiome. This describes ordered work,
// not an assertion that a phase's effects have already been executed.
// Timed event rewards remain an explicit dynamic slot even when none exist.
inline bool planClassicVictory(uint16_t wave, ClassicVictoryPlan& output) {
    output = {};
    if (!wave || wave > PokerogueContent::kClassicFinalWave) return false;
    output.completedWave = wave;
    auto push = [&](ClassicVictoryStep step) { output.steps[output.count++] = step; };
    push(ClassicVictoryStep::Experience);
    push(ClassicVictoryStep::BattleEnd);
    if (wave == PokerogueContent::kClassicFinalWave) {
        push(ClassicVictoryStep::GameClear);
        return true;
    }
    output.nextWave = static_cast<uint16_t>(wave + 1);
    push(ClassicVictoryStep::EggLapse);
    if (wave == 165) push(ClassicVictoryStep::LockCapsuleReward);
    push(ClassicVictoryStep::TimedEventRewards);
    push(wave % 10 ? ClassicVictoryStep::SelectModifier
                   : ClassicVictoryStep::FixedModifierRewards);
    if (wave % 10 == 0) push(ClassicVictoryStep::SelectBiome);
    push(ClassicVictoryStep::NewBattle);
    return true;
}

} // namespace Pokerogue3DS
