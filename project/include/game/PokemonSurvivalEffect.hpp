#pragma once
#include "game/PokemonBattleState.hpp"
namespace Pokerogue3DS {
// Pinned PreDefendFullHpEndureAbAttr and SturdyTag. Owned by the caller until
// actor persistence and turn-end tag dispatch are integrated.
struct PokemonSturdyPolicy {
    bool resolved = false;
    bool fullHpEndureAbilityActive = false;
    bool otherSurvivalEffectsResolved = false;
    bool boss = false;
};
inline bool resolvePokemonSturdyAbilityPolicy(uint16_t abilityId, bool abilityActive,
    bool otherSurvivalEffectsResolved, bool boss, PokemonSturdyPolicy& output) {
    const PokerogueContent::FullHpEndureAbilityProfile* found = nullptr;
    for (const auto& profile : PokerogueContent::kFullHpEndureAbilityProfiles) {
        if (profile.abilityId != abilityId) continue;
        if (found || !profile.resolved) return false;
        found = &profile;
    }
    if (!found) return false;
    PokemonSturdyPolicy policy{};
    policy.resolved = true;
    policy.fullHpEndureAbilityActive = abilityActive;
    policy.otherSurvivalEffectsResolved = otherSurvivalEffectsResolved;
    policy.boss = boss;
    output = policy;
    return true;
}
struct PokemonSturdyEvent {
    bool tagAdded = false;
    bool tagConsumed = false;
    uint16_t damageApplied = 0;
};
enum class PokemonSurvivalResult : uint8_t { Ok, UnresolvedPolicy, InvalidState };
inline PokemonSurvivalResult preparePokemonSturdyTag(const PokemonBattleState& target,
    uint32_t damage, const PokemonSturdyPolicy& policy, bool simulated,
    PokemonSturdyTagState& tag, PokemonSturdyEvent& output) {
    if (!policy.resolved) return PokemonSurvivalResult::UnresolvedPolicy;
    if (!target.maxHp || target.hp > target.maxHp) return PokemonSurvivalResult::InvalidState;
    PokemonSturdyEvent event{};
    // Simulated PreDefendFullHpEndureAbAttr does not create the tag or clamp damage.
    if (!simulated && policy.fullHpEndureAbilityActive && target.maxHp > 1 &&
        target.hp == target.maxHp && damage >= target.hp && !tag.present) {
        tag.present = true;
        event.tagAdded = true;
    }
    output = event;
    return PokemonSurvivalResult::Ok;
}
inline PokemonSurvivalResult applyPokemonSturdyDamage(PokemonBattleState& target,
    uint32_t damage, bool preventEndure, const PokemonSturdyPolicy& policy,
    PokemonSturdyTagState& tag, PokemonSturdyEvent& output) {
    if (!policy.resolved || !policy.otherSurvivalEffectsResolved || policy.boss)
        return PokemonSurvivalResult::UnresolvedPolicy;
    if (!target.maxHp || target.hp > target.maxHp) return PokemonSurvivalResult::InvalidState;
    PokemonSturdyEvent event{};
    if (target.hp) {
        if (!preventEndure && damage >= target.hp && target.hp == target.maxHp && tag.present) {
            damage = target.hp - 1;
            tag.present = false;
            event.tagConsumed = true;
        }
        event.damageApplied = damage < target.hp ? static_cast<uint16_t>(damage) : target.hp;
        target.hp -= event.damageApplied;
    }
    output = event;
    return PokemonSurvivalResult::Ok;
}
// SturdyTag is transient and lapses at TURN_END even when never consumed.
inline void lapsePokemonSturdyTurnEnd(PokemonSturdyTagState& tag) { tag.present = false; }
}
