#pragma once
#include "game/PokemonBattleState.hpp"
#include <cstring>

namespace Pokerogue3DS {
inline bool pokemonWeatherLifecycleSupported(uint16_t abilityId) {
    for (const auto& profile : PokerogueContent::kWeatherLifecycleAbilityProfiles)
        if (profile.abilityId == abilityId) return !profile.requiresDispatcher;
    return false;
}
inline const PokerogueContent::MoveWeatherChangeProfile* pokemonWeatherChangeProfile(uint16_t moveId) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || !move->target ||
        std::strcmp(move->target, "BOTH_SIDES") || move->accuracy != -1 ||
        move->attributeCount != 1 || !PokerogueContent::moveHasAttribute(*move, "WeatherChangeAttr")) return nullptr;
    const PokerogueContent::MoveWeatherChangeProfile* found = nullptr;
    for (const auto& row : PokerogueContent::kMoveWeatherChangeProfiles) {
        if (row.moveId != moveId) continue;
        if (found || !row.weatherType || row.weatherType > 9) return nullptr;
        found = &row;
    }
    return found;
}
struct PokemonWeatherPhaseEvent {
    PokemonWeatherDamageEvent player;
    PokemonWeatherDamageEvent enemy;
};
// Fresh single actors only: caller establishes absence of passives, weather tags,
// changing types and switch-out state. Canonical unsupported hooks fail explicitly.
inline bool applyPokemonSingleWeatherPhase(PokemonBattleState& player,
    PokemonBattleState& enemy, const PokemonArenaWeatherState& arena,
    bool upcomingInterlude, PokemonWeatherPhaseEvent& output) {
    PokemonWeatherPhaseEvent event{};
    if (upcomingInterlude || arena.type == PokemonEffectiveWeather::None) {
        output = event;
        return true;
    }
    if (!pokemonWeatherLifecycleSupported(player.abilityId) ||
        !pokemonWeatherLifecycleSupported(enemy.abilityId)) return false;
    const PokemonWeatherAbilityComponent components[] = {
        {player.abilityId, player.hp != 0, false},
        {enemy.abilityId, enemy.hp != 0, false}
    };
    PokemonWeatherResolutionPolicy suppression{};
    PokemonMoveWeatherContext context{};
    if (!composePokemonWeatherResolutionPolicy(components, 2, suppression) ||
        !resolvePokemonMoveWeatherContext(arena, suppression, context)) return false;
    auto nextPlayer = player;
    auto nextEnemy = enemy;
    const auto apply = [&](PokemonBattleState& actor, PokemonWeatherDamageEvent& result) {
        const auto* species = PokerogueContent::findSpeciesByDex(actor.speciesDex);
        const auto* form = actor.formId ? PokerogueContent::findFormById(actor.formId) : nullptr;
        if (!species || (actor.formId && !form)) return false;
        PokemonWeatherDamagePolicy policy{};
        policy.resolved = true;
        policy.weatherSuppressed = context.cancellationWeather == PokemonEffectiveWeather::None;
        policy.type1 = form ? form->type1 : species->type1;
        policy.type2 = form ? form->type2 : species->type2;
        if (!pokemonAbilityBlocksWeatherDamage(actor.abilityId, arena.type, policy.abilityBlocksDamage)) return false;
        return applyPokemonWeatherResidualDamage(actor, arena, policy, result);
    };
    if (!apply(nextPlayer, event.player) || !apply(nextEnemy, event.enemy)) return false;
    player = nextPlayer;
    enemy = nextEnemy;
    output = event;
    return true;
}
} // namespace Pokerogue3DS
