#pragma once
#include "game/PokemonSurvivalEffect.hpp"
#include <cmath>
#include <cstring>
namespace Pokerogue3DS {
inline const PokerogueContent::MoveRecoilProfile* canonicalRecoilProfile(uint16_t id) {
    const PokerogueContent::MoveRecoilProfile* found = nullptr;
    for (const auto& row : PokerogueContent::kMoveRecoilProfiles) {
        if (row.moveId != id) continue;
        if (found || !std::isfinite(row.ratio) || row.ratio <= 0 || row.ratio > 1) return nullptr;
        found = &row;
    }
    return found;
}
inline const PokerogueContent::MoveRecoilProfile* damageRecoilProfile(uint16_t id) {
    const auto* move = PokerogueContent::findMoveById(id);
    if (!move || move->category == PokerogueContent::MoveStatus || move->power <= 0 ||
        !move->target || std::strcmp(move->target, "NEAR_OTHER") || move->upstreamFlags != 0 ||
        move->attributeCount != 1 || !PokerogueContent::moveHasAttribute(*move, "RecoilAttr")) return nullptr;
    return canonicalRecoilProfile(id);
}
struct PokemonRecoilPolicy {
    bool resolved = false;
    bool abilityBlocksRecoil = false;
    bool abilityBlocksIndirectDamage = false;
};
struct PokemonRecoilEvent { uint16_t damage = 0; bool blocked = false; bool fainted = false; };
enum class PokemonRecoilResult : uint8_t { Ok, UnresolvedPolicy, InvalidState, UnsupportedMove };
// Pinned RecoilAttr: total damage of the move, not its power or uncapped roll.
// For multi-hit callers this must be the accumulated damage and the last hit.
inline PokemonRecoilResult applyPokemonRecoil(PokemonBattleState& user, uint16_t moveId,
    uint32_t totalDamageDealt, bool moveSucceeded, const PokemonRecoilPolicy& policy,
    PokemonRecoilEvent& output) {
    if (!policy.resolved) return PokemonRecoilResult::UnresolvedPolicy;
    const auto* profile = canonicalRecoilProfile(moveId);
    if (!profile) return PokemonRecoilResult::UnsupportedMove;
    if (!user.maxHp || user.hp > user.maxHp) return PokemonRecoilResult::InvalidState;
    PokemonRecoilEvent event{};
    if (!user.hp || (!profile->useMaxHp && !totalDamageDealt) ||
        (profile->useMaxHp && !moveSucceeded)) { output = event; return PokemonRecoilResult::Ok; }
    if (!profile->unblockable && (policy.abilityBlocksRecoil || policy.abilityBlocksIndirectDamage))
        event.blocked = true;
    else {
        const double amount = std::fmax(std::floor((profile->useMaxHp ? user.maxHp : totalDamageDealt) * profile->ratio), 1.0);
        const uint32_t requested = amount >= user.hp ? user.hp : static_cast<uint16_t>(amount);
        if (applyPokemonExistingSturdyDamage(user, requested, event.damage) != PokemonSurvivalResult::Ok)
            return PokemonRecoilResult::InvalidState;
        event.fainted = !user.hp;
    }
    output = event;
    return PokemonRecoilResult::Ok;
}
inline PokemonRecoilPolicy canonicalFreshActorRecoilPolicy(uint16_t abilityId) {
    PokemonRecoilPolicy policy{};
    policy.resolved = true; // Fresh applicable primary ability; no passives/suppression.
    for (const auto& row : PokerogueContent::kRecoilAbilityProfiles) {
        if (row.abilityId != abilityId) continue;
        policy.abilityBlocksRecoil = row.blocksRecoil;
        policy.abilityBlocksIndirectDamage = row.blocksIndirectDamage;
    }
    return policy;
}
inline double canonicalRecoilAiBenefit(const PokerogueContent::Move& move) {
    return canonicalRecoilProfile(move.id) && PokerogueContent::moveHasAttribute(move, "RecoilAttr")
        ? std::floor(move.power / 5.0 / -4.0) : 0;
}
}
