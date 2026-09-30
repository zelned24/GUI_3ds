#pragma once

#include "game/PokerogueRngAdapter.hpp"
#include <cstddef>
#include <cstdint>

namespace Pokerogue3DS {

// Recreates Battle.battleSeed creation and Battle.randSeedInt's per-turn
// stream. This remains separate from the encounter stream; the seed, turn and
// current PokerogueRngState can be persisted by the eventual full-run schema.
class PokerogueBattleRng {
public:
    static constexpr std::size_t kBattleSeedLength = 16;

    bool initialize(const uint16_t* rootSeed, std::size_t seedLength, uint32_t waveIndex) {
        m_ready = false;
        if (seedLength > PokerogueRngAdapter::kMaxSeedCodeUnits || (seedLength && !rootSeed)) return false;

        uint16_t waveSeed[PokerogueRngAdapter::kMaxSeedCodeUnits]{};
        if (!PokerogueRngAdapter::shiftCharCodes(rootSeed, seedLength, waveIndex,
                waveSeed, PokerogueRngAdapter::kMaxSeedCodeUnits)) return false;

        PokerogueRngAdapter constructorRng;
        PokerogueSeedOffsetScope battleScope(constructorRng, waveSeed, seedLength, waveIndex << 3);
        if (!battleScope.valid()) return false;

        static constexpr char kSeedCharacters[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        for (std::size_t i = 0; i < kBattleSeedLength; ++i) {
            const auto index = constructorRng.randSeedInt(62);
            if (index < 0 || index >= 62) return false;
            m_battleSeed[i] = static_cast<uint16_t>(kSeedCharacters[index]);
        }
        m_ready = true;
        return true;
    }

    bool beginTurn(uint32_t turn) {
        if (!m_ready || !turn) return false;
        uint16_t turnSeed[kBattleSeedLength]{};
        if (!PokerogueRngAdapter::shiftCharCodes(m_battleSeed, kBattleSeedLength,
                turn << 6, turnSeed, kBattleSeedLength)) return false;
        m_rng.sow(turnSeed, kBattleSeedLength);
        m_turn = turn;
        return true;
    }

    bool randSeedInt(int32_t range, int32_t& output, int32_t min = 0) {
        if (!m_turn) return false;
        output = m_rng.randSeedInt(range, min);
        return true;
    }

    PokerogueRngState state() const { return m_rng.state(); }
    void restoreState(const PokerogueRngState& state) { m_rng.restore(state); }
    PokerogueRngAdapter* currentStream() { return m_turn ? &m_rng : nullptr; }
    uint32_t turn() const { return m_turn; }
    bool ready() const { return m_ready; }
    const uint16_t* battleSeed() const { return m_battleSeed; }

private:
    uint16_t m_battleSeed[kBattleSeedLength]{};
    PokerogueRngAdapter m_rng{};
    uint32_t m_turn = 0;
    bool m_ready = false;
};

} // namespace Pokerogue3DS
