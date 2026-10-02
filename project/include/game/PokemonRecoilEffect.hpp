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
inline bool pokemonMovePpExhausted(const PokemonBattleState& actor) {
    if (!actor.moveCount || actor.moveCount > 4) return false;
    for (uint8_t i = 0; i < actor.moveCount; ++i) {
        const auto& slot = actor.moves[i];
        if (!PokerogueContent::findMoveById(slot.moveId) || slot.pp > slot.maxPp || slot.pp) return false;
    }
    return true;
}
struct PokemonStruggleActionResult {
    PokemonMoveActionResult attack{};
    PokemonRecoilEvent recoil{};
};
// Virtual command: no Struggle slot is inserted into the persistent moveset.
// Caller resolves pre-move restrictions and random target selection separately.
inline PokemonMoveActionStatus usePokemonStruggleCommand(PokemonBattleState& user,
    PokemonBattleState& target, PokerogueRngAdapter& rng, PokemonStruggleActionResult& output,
    const PokemonMoveWeatherContext* weather = nullptr, const PokemonCriticalPolicy* critical = nullptr,
    const PokemonHitPolicy* hit = nullptr, PokemonBossState* targetBoss = nullptr,
    const PokemonBossDamagePolicy* bossPolicy = nullptr, PokerogueRngAdapter* globalRng = nullptr,
    const PokemonBurnDamagePolicy* burn = nullptr, PokemonBossState* userBoss = nullptr,
    const PokemonBossDamagePolicy* userBossPolicy = nullptr) {
    if (&user == &target || !user.hp || !user.maxHp || user.hp > user.maxHp ||
        !PokerogueContent::kStruggleDefinitionResolved || !pokemonMovePpExhausted(user))
        return PokemonMoveActionStatus::InvalidMoveSlot;
    const auto* move = PokerogueContent::findMoveById(PokerogueContent::kStruggleMoveId);
    const auto* recoil = canonicalRecoilProfile(PokerogueContent::kStruggleMoveId);
    if (!move || !recoil || !recoil->useMaxHp || !recoil->unblockable || recoil->ratio != 0.25)
        return PokemonMoveActionStatus::DamageResolutionFailed;
    if ((targetBoss != nullptr) != (bossPolicy != nullptr) ||
        (targetBoss && (!globalRng || globalRng == &rng))) return PokemonMoveActionStatus::UnresolvedBoss;
    if ((userBoss != nullptr) != (userBossPolicy != nullptr) ||
        (userBoss && (!globalRng || globalRng == &rng || userBoss == targetBoss ||
            !userBossPolicy->resolved || !userBossPolicy->damageCallbacksResolved ||
            !userBoss->segmentCount || userBoss->segmentIndex >= userBoss->segmentCount)))
        return PokemonMoveActionStatus::UnresolvedBoss;
    auto nextUser = user, nextTarget = target;
    PokemonBossState nextBoss{};
    PokerogueRngAdapter nextGlobal;
    PokemonBossState nextUserBoss{};
    if (targetBoss) nextBoss = *targetBoss;
    if (userBoss) nextUserBoss = *userBoss;
    if (targetBoss || userBoss) nextGlobal = *globalRng;
    auto nextRng = rng;
    nextUser.moveCount = 1;
    nextUser.moves[0] = {PokerogueContent::kStruggleMoveId, 1, 1};
    const PokemonPpPolicy pp{true, 0};
    PokemonStruggleActionResult event{};
    const auto result = useStandardPokemonMove(nextUser, nextTarget, 0, true, nextRng, event.attack,
        weather, critical, hit, &pp, targetBoss ? &nextBoss : nullptr, bossPolicy,
        targetBoss ? &nextGlobal : nullptr, burn);
    if (result != PokemonMoveActionStatus::Ok) return result;
    const uint16_t recoilHp = nextUser.hp;
    const auto recoilSturdy = nextUser.sturdy;
    const auto recoilPolicy = canonicalFreshActorRecoilPolicy(user.abilityId);
    if (applyPokemonRecoil(nextUser, move->id, event.attack.damageApplied,
        event.attack.damageRoll.hit && !event.attack.weatherCancelled, recoilPolicy, event.recoil) != PokemonRecoilResult::Ok)
        return PokemonMoveActionStatus::DamageResolutionFailed;
    if (userBoss && recoilHp && event.attack.damageRoll.hit && !event.attack.weatherCancelled) {
        nextUser.hp = recoilHp;
        nextUser.sturdy = recoilSturdy;
        auto policy = *userBossPolicy;
        policy.ignoreSegments = true; // Pinned RecoilAttr.damageAndUpdate.
        policy.preventEndure = false;
        const auto requested = static_cast<uint32_t>(std::fmax(std::floor(nextUser.maxHp * recoil->ratio), 1.0));
        PokemonBossDamageEvent damage{};
        if (!applyPokemonBossDamage(nextUser, nextUserBoss, requested, policy, nextGlobal, damage))
            return PokemonMoveActionStatus::UnresolvedBoss;
        event.recoil.damage = damage.damageApplied;
        event.recoil.fainted = !nextUser.hp;
    }
    nextUser.moveCount = user.moveCount;
    for (uint8_t i = 0; i < 4; ++i) nextUser.moves[i] = user.moves[i];
    user = nextUser;
    target = nextTarget;
    rng = nextRng;
    if (targetBoss) *targetBoss = nextBoss;
    if (userBoss) *userBoss = nextUserBoss;
    if (targetBoss || userBoss) *globalRng = nextGlobal;
    output = event;
    return PokemonMoveActionStatus::Ok;
}

}
