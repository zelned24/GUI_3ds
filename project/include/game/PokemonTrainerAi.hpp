#pragma once

#include "game/PokerogueRngAdapter.hpp"
#include <cmath>
#include <cstdint>

namespace Pokerogue3DS {

// EnemyPokemon.getNextMove uses this score-dependent walk for SMART trainers.
// The input scores are in moveset order, after target scoring and any KO-only
// filtering. Stable sorting keeps upstream order when two scores are equal.
inline bool selectSmartTrainerMoveSlot(const double* scores, const uint8_t* slots,
                                       uint8_t count, PokerogueRngAdapter& battleRng,
                                       uint8_t& selectedSlot) {
    if (!scores || !slots || !count || count > 4) return false;
    uint8_t ordered[4]{};
    for (uint8_t i = 0; i < count; ++i) {
        if (!std::isfinite(scores[i])) return false;
        ordered[i] = i;
        uint8_t position = i;
        while (position && scores[ordered[position]] > scores[ordered[position - 1]]) {
            const uint8_t previous = ordered[position - 1];
            ordered[position - 1] = ordered[position];
            ordered[position] = previous;
            --position;
        }
    }
    uint8_t chosen = 0;
    while (chosen + 1 < count) {
        const double current = scores[ordered[chosen]];
        const double next = scores[ordered[chosen + 1]];
        const double ratio = next / current;
        // JavaScript's NaN >= 0 is false. Infinity is allowed when the
        // current score is zero and the following score is positive.
        if (!(ratio >= 0.0)) break;
        const int32_t threshold = ratio >= 2.0 ? 101
            : static_cast<int32_t>(std::floor(ratio * 50.0 + 0.5));
        if (battleRng.randSeedInt(100) >= threshold) break;
        ++chosen;
    }
    selectedSlot = slots[ordered[chosen]];
    return true;
}

} // namespace Pokerogue3DS
