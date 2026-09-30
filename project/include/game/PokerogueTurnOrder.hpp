#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Pokerogue3DS {

enum class BaselineFirstMover : uint8_t { Invalid, Player, Enemy };
struct PokemonTurnOrderFieldPolicy {
    bool resolved = false;
    bool speedReversed = false; // Resolved TrickRoomTag application.
};


struct PokemonTrickRoomState {
    uint16_t turnsLeft = 0;
    uint16_t maxDuration = 0;
    uint16_t sourceMoveId = 0;
    uint32_t sourcePokemonId = 0;
};
inline bool validPokemonTrickRoomState(const PokemonTrickRoomState& state) {
    if (!state.turnsLeft)
        return !state.maxDuration && !state.sourceMoveId && !state.sourcePokemonId;
    if (state.turnsLeft > state.maxDuration) return false;
    for (const auto& profile : PokerogueContent::kTrickRoomMoveProfiles)
        if (profile.moveId == state.sourceMoveId && profile.duration == state.maxDuration)
            return true;
    return false;
}
struct PokemonTrickRoomEvent {
    bool activated = false;
    bool removed = false;
    bool expired = false;
};
inline bool applyPokemonTrickRoomMove(PokemonTrickRoomState& state, uint16_t moveId,
    uint32_t sourcePokemonId, PokemonTrickRoomEvent& output) {
    if (!validPokemonTrickRoomState(state)) return false;
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category != PokerogueContent::MoveStatus) return false;
    for (const auto& profile : PokerogueContent::kTrickRoomMoveProfiles) {
        if (profile.moveId != moveId) continue;
        PokemonTrickRoomEvent event{};
        if (state.turnsLeft) {
            state = {};
            event.removed = true; // RoomArenaTag.onOverlap removes, not refreshes.
        } else {
            state = {profile.duration, profile.duration, moveId, sourcePokemonId};
            event.activated = true;
        }
        output = event;
        return true;
    }
    return false;
}
inline PokemonTurnOrderFieldPolicy pokemonTrickRoomOrderPolicy(const PokemonTrickRoomState& state);
enum class PokemonTrickRoomCommandResult : uint8_t {
    Ok, InvalidActor, InvalidSlot, InvalidDefinition, NoPp, UnresolvedPolicy, InvalidField
};
struct PokemonTrickRoomCommandPolicy {
    bool resolved = false; // Pre-move status/tag/condition checks already resolved.
    bool failsBeforeEffect = false;
    uint8_t ppCost = 1; // Resolved Pressure/ignore-PP policy.
};
struct PokemonTrickRoomCommandEvent {
    bool failed = false;
    uint8_t ppConsumed = 0;
    PokemonTrickRoomEvent field{};
};
inline PokemonTrickRoomCommandResult usePokemonTrickRoomCommand(
    PokemonBattleState& user, PokemonTrickRoomState& state, uint8_t moveSlot,
    const PokemonTrickRoomCommandPolicy& policy, PokemonTrickRoomCommandEvent& output) {
    if (!policy.resolved) return PokemonTrickRoomCommandResult::UnresolvedPolicy;
    if (!user.hp || !user.maxHp || user.hp > user.maxHp) return PokemonTrickRoomCommandResult::InvalidActor;
    if (moveSlot >= 4 || moveSlot >= user.moveCount) return PokemonTrickRoomCommandResult::InvalidSlot;
    const auto& slot = user.moves[moveSlot];
    const auto* move = PokerogueContent::findMoveById(slot.moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || !move->target ||
        std::strcmp(move->target, "BOTH_SIDES") || move->attributeCount != 1 ||
        !PokerogueContent::moveHasAttribute(*move, "AddArenaTagAttr"))
        return PokemonTrickRoomCommandResult::InvalidDefinition;
    bool represented = false;
    for (const auto& profile : PokerogueContent::kTrickRoomMoveProfiles)
        if (profile.moveId == move->id) represented = true;
    if (!represented) return PokemonTrickRoomCommandResult::InvalidDefinition;
    if (!slot.pp && policy.ppCost) return PokemonTrickRoomCommandResult::NoPp;
    if (slot.pp > slot.maxPp) return PokemonTrickRoomCommandResult::InvalidActor;
    if (!pokemonTrickRoomOrderPolicy(state).resolved) return PokemonTrickRoomCommandResult::InvalidField;
    PokemonTrickRoomState next = state;
    PokemonTrickRoomCommandEvent event{};
    event.failed = policy.failsBeforeEffect;
    if (!event.failed && !applyPokemonTrickRoomMove(next, move->id, user.pokemonId, event.field))
        return PokemonTrickRoomCommandResult::InvalidField;
    event.ppConsumed = policy.ppCost < slot.pp ? policy.ppCost : slot.pp;
    user.moves[moveSlot].pp = static_cast<uint8_t>(slot.pp - event.ppConsumed);
    state = next;
    output = event;
    // BOTH_SIDES field hit check bypasses accuracy/type/critical RNG.
    return PokemonTrickRoomCommandResult::Ok;
}

inline bool advancePokemonTrickRoomTurnEnd(PokemonTrickRoomState& state,
    PokemonTrickRoomEvent& output) {
    if (!validPokemonTrickRoomState(state)) return false;
    PokemonTrickRoomEvent event{};
    if (state.turnsLeft && --state.turnsLeft == 0) {
        state = {};
        event.removed = event.expired = true;
    }
    output = event;
    return true;
}
inline PokemonTurnOrderFieldPolicy pokemonTrickRoomOrderPolicy(const PokemonTrickRoomState& state) {
    PokemonTurnOrderFieldPolicy policy{};
    policy.resolved = validPokemonTrickRoomState(state);
    policy.speedReversed = state.turnsLeft != 0;
    return policy;
}

// Pinned MovePhasePriorityQueue sorts by move priority after
// sortInSpeedOrder. For a two-Pokemon field, that speed sort shuffles the
// initial [player, enemy] order with a stream derived from waveSeed and
// turn * 1000 + 2, then sorts descending by effective speed. This bounded
// resolver applies speed stages and resolved weather abilities. Optional field
// policy reverses speed order; held items/terrain/priority modifiers remain separate.
inline BaselineFirstMover resolveBaselineFirstMover(
    const PokemonBattleState& player, const PokemonBattleState& enemy,
    uint16_t playerMoveId, uint16_t enemyMoveId,
    const uint16_t* rootSeed, std::size_t seedLength,
    uint16_t wave, uint32_t turn,
    const PokemonMoveWeatherContext* resolvedArenaWeather = nullptr,
    const PokemonTurnOrderFieldPolicy* fieldPolicy = nullptr) {
  if (!rootSeed || !seedLength || seedLength > PokerogueRngAdapter::kMaxSeedCodeUnits
      || !wave || !turn || turn > (0xffffffffU - 2U) / 1000U
      || !player.stats[5] || !enemy.stats[5]) return BaselineFirstMover::Invalid;
  if (fieldPolicy && !fieldPolicy->resolved) return BaselineFirstMover::Invalid;
  const bool reverseSpeed = fieldPolicy && fieldPolicy->speedReversed;
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
  if (playerSpeed != enemySpeed) {
    const bool playerFirst = reverseSpeed ? playerSpeed < enemySpeed : playerSpeed > enemySpeed;
    return playerFirst ? BaselineFirstMover::Player : BaselineFirstMover::Enemy;
  }
  // Upstream reverses the whole stable sorted list, including shuffled ties.
  const bool enemyFirst = reverseSpeed ? !enemyWasShuffledFirst : enemyWasShuffledFirst;
  return enemyFirst ? BaselineFirstMover::Enemy : BaselineFirstMover::Player;
}

} // namespace Pokerogue3DS
