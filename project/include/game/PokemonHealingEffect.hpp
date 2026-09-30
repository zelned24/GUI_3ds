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
}
