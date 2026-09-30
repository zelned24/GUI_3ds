#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"

#include <cstddef>
#include <cstdint>

namespace Pokerogue3DS {

enum class BaselineFirstMover : uint8_t { Invalid, Player, Enemy };

// Pinned MovePhasePriorityQueue sorts by move priority after
// sortInSpeedOrder. For a two-Pokemon field, that speed sort shuffles the
// initial [player, enemy] order with a stream derived from waveSeed and
// turn * 1000 + 2, then sorts descending by effective speed. This bounded
// resolver applies canonical speed stages; abilities, held items, terrain,
// Trick Room and priority modifiers require their own native rules.
inline BaselineFirstMover resolveBaselineFirstMover(
    const PokemonBattleState& player, const PokemonBattleState& enemy,
    uint16_t playerMoveId, uint16_t enemyMoveId,
    const uint16_t* rootSeed, std::size_t seedLength,
    uint16_t wave, uint32_t turn,
    const PokemonMoveWeatherContext* resolvedArenaWeather = nullptr) {
  if (!rootSeed || !seedLength || seedLength > PokerogueRngAdapter::kMaxSeedCodeUnits
      || !wave || !turn || turn > (0xffffffffU - 2U) / 1000U
      || !player.stats[5] || !enemy.stats[5]) return BaselineFirstMover::Invalid;
  const auto* playerMove = PokerogueContent::findMoveById(playerMoveId);
  const auto* enemyMove = PokerogueContent::findMoveById(enemyMoveId);
  if (!playerMove || !enemyMove) return BaselineFirstMover::Invalid;

  uint32_t playerSpeed = 0, enemySpeed = 0;
  if (!pokemonBaselineEffectiveStat(player, 5, false, playerSpeed) ||
      !pokemonBaselineEffectiveStat(enemy, 5, false, enemySpeed)) return BaselineFirstMover::Invalid;
  if (resolvedArenaWeather &&
      (!pokemonWeatherEffectiveSpeed(player, *resolvedArenaWeather, playerSpeed) ||
       !pokemonWeatherEffectiveSpeed(enemy, *resolvedArenaWeather, enemySpeed))) return BaselineFirstMover::Invalid;
  uint16_t waveSeed[PokerogueRngAdapter::kMaxSeedCodeUnits]{};
  if (!PokerogueRngAdapter::shiftCharCodes(rootSeed, seedLength, wave,
          waveSeed, PokerogueRngAdapter::kMaxSeedCodeUnits)) return BaselineFirstMover::Invalid;
  PokerogueRngAdapter tieRng;
  PokerogueSeedOffsetScope tieScope(tieRng, waveSeed, seedLength, turn * 1000U + 2U);
  if (!tieScope.valid()) return BaselineFirstMover::Invalid;
  // Fisher-Yates always runs before speed comparison, even without a tie.
  const bool enemyWasShuffledFirst = tieRng.integerInRange(0, 1) == 0;
  if (playerMove->priority != enemyMove->priority)
    return playerMove->priority > enemyMove->priority
        ? BaselineFirstMover::Player : BaselineFirstMover::Enemy;
  if (playerSpeed != enemySpeed)
    return playerSpeed > enemySpeed
        ? BaselineFirstMover::Player : BaselineFirstMover::Enemy;
  return enemyWasShuffledFirst ? BaselineFirstMover::Enemy : BaselineFirstMover::Player;
}

} // namespace Pokerogue3DS
