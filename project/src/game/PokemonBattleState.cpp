#include "game/PokemonBattleState.hpp"

namespace Pokerogue3DS {
namespace {
bool abilityBelongsToSpecies(const PokerogueContent::Species& species, uint16_t id) {
    return id != 0 && (species.ability1 == id || species.ability2 == id ||
        species.abilityHidden == id || species.abilityPassive == id);
}
bool abilityBelongsToForm(const PokerogueContent::Form& form, uint16_t id) {
    return id != 0 && (form.ability1 == id || form.ability2 == id || form.abilityHidden == id);
}

uint16_t calculatedStat(uint8_t base, uint8_t iv, uint16_t level,
                        uint8_t statIndex, int8_t raised, int8_t lowered) {
    const uint32_t scaled = ((2u * base + iv) * level) / 100u;
    if (statIndex == 0) return static_cast<uint16_t>(scaled + level + 10u);

    uint32_t value = scaled + 5u;
    if (raised != lowered && raised == static_cast<int8_t>(statIndex)) value = (value * 11u + 9u) / 10u;
    else if (raised != lowered && lowered == static_cast<int8_t>(statIndex)) value = (value * 9u) / 10u;
    return static_cast<uint16_t>(value ? value : 1u);
}
}

PokemonBattleInitResult initializePokemonBattleState(
    const PokemonBattleInit& input,
    PokemonBattleState& output) {
    const auto* species = PokerogueContent::findSpeciesByDex(input.speciesDex);
    if (!species) return PokemonBattleInitResult::MissingSpecies;
    const auto* form = input.formId
        ? PokerogueContent::findFormById(input.formId)
        : (species->firstFormId[0] ? PokerogueContent::findFormById(species->firstFormId) : nullptr);
    if (input.formId && !form) return PokemonBattleInitResult::InvalidForm;
    if (form) {
        const char* speciesId = species->id;
        const char* formSpeciesId = form->speciesId;
        while (*speciesId && *formSpeciesId && *speciesId == *formSpeciesId) { ++speciesId; ++formSpeciesId; }
        if (*speciesId != *formSpeciesId) return PokemonBattleInitResult::InvalidForm;
    }
    if (input.level == 0 || input.level > 100) return PokemonBattleInitResult::InvalidLevel;
    for (uint8_t iv : input.ivs) if (iv > 31) return PokemonBattleInitResult::InvalidIv;
    const auto validNatureStat = [](int8_t stat) { return stat == -1 || (stat >= 1 && stat <= 5); };
    if (!validNatureStat(input.natureRaisedStat) || !validNatureStat(input.natureLoweredStat)) {
        return PokemonBattleInitResult::InvalidNatureStat;
    }
    if (form ? !abilityBelongsToForm(*form, input.abilityId) : !abilityBelongsToSpecies(*species, input.abilityId)) return PokemonBattleInitResult::InvalidAbility;
    if (input.moveCount > 4) return PokemonBattleInitResult::InvalidMoveCount;

    PokemonBattleState next{};
    next.speciesDex = input.speciesDex;
    next.formId = form ? form->id : nullptr;
    next.level = input.level;
    next.abilityId = input.abilityId;
    const uint8_t baseStats[6] = {form ? form->hp : species->hp, form ? form->atk : species->atk,
        form ? form->def : species->def, form ? form->spatk : species->spatk,
        form ? form->spdef : species->spdef, form ? form->speed : species->speed};
    for (uint8_t index = 0; index < 6; ++index) {
        next.stats[index] = calculatedStat(baseStats[index], input.ivs[index], input.level,
            index, input.natureRaisedStat, input.natureLoweredStat);
    }
    next.maxHp = next.stats[0];
    next.hp = next.maxHp;
    next.moveCount = input.moveCount;
    for (uint8_t index = 0; index < input.moveCount; ++index) {
        const auto* move = PokerogueContent::findMoveById(input.moveIds[index]);
        if (!move) return PokemonBattleInitResult::MissingMove;
        if (move->pp < 1 || move->pp > 255) return PokemonBattleInitResult::InvalidMovePp;
        next.moves[index] = {move->id, static_cast<uint8_t>(move->pp), static_cast<uint8_t>(move->pp)};
    }
    output = next;
    return PokemonBattleInitResult::Ok;
}

} // namespace Pokerogue3DS
