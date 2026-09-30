#pragma once
#include "game/PokemonBattleState.hpp"
#include <cmath>
#include <cstring>

namespace Pokerogue3DS {
// Pinned src/data/moves/move.ts: HealAttr and PokemonHealPhase.
// Constant self healing only. Rest/variable healing/drain are distinct effects.
inline const PokerogueContent::MoveHealProfile* selfHealingProfile(uint16_t moveId) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || !move->target ||
        std::strcmp(move->target, "USER") || move->attributeCount != 1 ||
        move->upstreamFlags != 0 || !PokerogueContent::moveHasAttribute(*move, "HealAttr")) return nullptr;
    const PokerogueContent::MoveHealProfile* found = nullptr;
    for (const auto& profile : PokerogueContent::kMoveHealProfiles) {
        if (profile.moveId != moveId) continue;
        if (found || !profile.selfTarget || !std::isfinite(profile.ratio) ||
            profile.ratio <= 0 || profile.ratio > 1) return nullptr;
        found = &profile;
    }
    return found;
}
struct PokemonHealingPolicy {
    bool resolved = false; // Caller resolves move blocks, healing abilities/items and tags.
    bool blockedBeforeMove = false;
    bool healBlocked = false;
    double ratioMultiplier = 1.0; // MoveHealBoostAbAttr before half-up rounding.
    double healingMultiplier = 1.0; // HealingBoosterModifier after base rounding, floored.
};
enum class PokemonHealingResult : uint8_t { Ok, InvalidState, UnsupportedMove, UnresolvedPolicy };
struct PokemonHealingEvent {
    bool failedFullHp = false;
    bool blocked = false;
    bool showAnimation = false;
    uint8_t ppSpent = 0;
    uint16_t hpBefore = 0;
    uint16_t hpAfter = 0;
    uint16_t healed = 0;
};
inline PokemonHealingResult usePokemonSelfHealingCommand(PokemonBattleState& user,
    uint8_t slot, const PokemonHealingPolicy& policy, PokemonHealingEvent& output) {
    if (!policy.resolved) return PokemonHealingResult::UnresolvedPolicy;
    if (!user.hp || !user.maxHp || user.hp > user.maxHp || slot >= user.moveCount || slot >= 4 ||
        !user.moves[slot].pp || user.moves[slot].pp > user.moves[slot].maxPp ||
        !std::isfinite(policy.ratioMultiplier) || policy.ratioMultiplier <= 0 ||
        !std::isfinite(policy.healingMultiplier) || policy.healingMultiplier <= 0)
        return PokemonHealingResult::InvalidState;
    const auto* profile = selfHealingProfile(user.moves[slot].moveId);
    if (!profile) return PokemonHealingResult::UnsupportedMove;
    const double baseAmount = std::floor(user.maxHp * profile->ratio * policy.ratioMultiplier + 0.5);
    const double amount = std::floor(baseAmount * policy.healingMultiplier);
    if (!std::isfinite(amount)) return PokemonHealingResult::InvalidState;
    PokemonHealingEvent event{};
    event.hpBefore = event.hpAfter = user.hp;
    event.showAnimation = profile->showAnimation;
    event.blocked = policy.blockedBeforeMove;
    if (!policy.blockedBeforeMove) {
        // USER has no opponent target; Pressure does not increase this cost.
        --user.moves[slot].pp;
        event.ppSpent = 1;
        event.failedFullHp = profile->failOnFullHp && user.hp == user.maxHp;
        event.blocked = !event.failedFullHp && policy.healBlocked;
        if (!event.failedFullHp && !event.blocked) {
            const uint16_t missing = user.maxHp - user.hp;
            event.healed = amount >= missing ? missing : static_cast<uint16_t>(amount);
            user.hp += event.healed;
            event.hpAfter = user.hp;
        }
    }
    output = event;
    return PokemonHealingResult::Ok;
}
inline bool canonicalSelfHealingAiScore(const PokemonBattleState& user, uint16_t moveId, double& score) {
    const auto* profile = selfHealingProfile(moveId);
    if (!profile || !user.maxHp || user.hp > user.maxHp) return false;
    const double raw = (1.0 - static_cast<double>(user.hp) / user.maxHp) * 20.0 - profile->ratio * 10.0;
    score = std::floor(raw / (1.0 - profile->ratio / 2.0) + 0.5);
    return true;
}
inline const PokerogueContent::MoveDrainProfile* damageDrainProfile(uint16_t id) {
    const auto* move = PokerogueContent::findMoveById(id);
    if (!move || move->category == PokerogueContent::MoveStatus || move->power <= 0 ||
        !move->target || std::strcmp(move->target, "NEAR_OTHER") || move->upstreamFlags != 0 ||
        move->attributeCount != 1 || !PokerogueContent::moveHasAttribute(*move, "HitHealAttr")) return nullptr;
    const PokerogueContent::MoveDrainProfile* found = nullptr;
    for (const auto& profile : PokerogueContent::kMoveDrainProfiles) {
        if (profile.moveId != id) continue;
        if (found || !std::isfinite(profile.ratio) || profile.ratio <= 0 || profile.ratio > 1) return nullptr;
        found = &profile;
    }
    return found;
}
inline bool hasCanonicalReverseDrain(uint16_t abilityId) {
    for (const auto& profile : PokerogueContent::kReverseDrainProfiles)
        if (profile.abilityId == abilityId) return true;
    return false;
}
struct PokemonDrainPolicy {
    bool resolved = false;
    bool healBlocked = false;
    bool reverseDrain = false;
    bool indirectDamageBlocked = false;
    double healingMultiplier = 1.0;
};
struct PokemonDrainEvent { uint16_t healed = 0; uint16_t reversedDamage = 0; bool blocked = false; };
inline PokemonHealingResult applyPokemonDamageDrain(PokemonBattleState& user, uint16_t moveId,
    uint16_t damageApplied, const PokemonDrainPolicy& policy, PokemonDrainEvent& output) {
    if (!policy.resolved) return PokemonHealingResult::UnresolvedPolicy;
    const auto* profile = damageDrainProfile(moveId);
    if (!profile) return PokemonHealingResult::UnsupportedMove;
    if (!user.maxHp || user.hp > user.maxHp || !std::isfinite(policy.healingMultiplier) ||
        policy.healingMultiplier <= 0) return PokemonHealingResult::InvalidState;
    PokemonDrainEvent event{};
    if (!user.hp || !damageApplied) { output = event; return PokemonHealingResult::Ok; }
    const double base = std::fmax(std::floor(damageApplied * profile->ratio), 1.0);
    const double amount = policy.reverseDrain ? std::ceil(base * policy.healingMultiplier)
        : std::floor(base * policy.healingMultiplier);
    if (!std::isfinite(amount)) return PokemonHealingResult::InvalidState;
    if (policy.reverseDrain) {
        // ReverseDrainAbAttr suppresses HitHealAttr even if indirect damage is blocked.
        event.blocked = policy.indirectDamageBlocked;
        if (!event.blocked) {
            event.reversedDamage = amount >= user.hp ? user.hp : static_cast<uint16_t>(amount);
            user.hp -= event.reversedDamage;
        }
    } else if (policy.healBlocked) event.blocked = true;
    else {
        const uint16_t missing = user.maxHp - user.hp;
        event.healed = amount >= missing ? missing : static_cast<uint16_t>(amount);
        user.hp += event.healed;
    }
    output = event;
    return PokemonHealingResult::Ok;
}
inline double canonicalDamageDrainAiBenefit(const PokemonBattleState& user,
    const PokerogueContent::Move& move) {
    if (!user.maxHp || !damageDrainProfile(move.id)) return 0;
    return std::floor(std::fmax(1.0 - static_cast<double>(user.hp) / user.maxHp - 0.33, 0.0) * (move.power / 4.0));
}

}
