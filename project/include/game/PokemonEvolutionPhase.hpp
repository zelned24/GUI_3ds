#pragma once

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokemonBattleState.hpp"
#include <cstdint>
#include <cstring>
#include <string>
#include <algorithm>

namespace Pokerogue3DS {

inline bool pokemonEvolutionTextEqual(const char* left, const char* right) {
    if (!left || !right) return left == right;
    while (*left && *right && *left == *right) { ++left; ++right; }
    return *left == *right;
}

inline const PokerogueContent::Species* findSpeciesById(const char* speciesId) {
    if (!speciesId) return nullptr;
    for (std::size_t i = 0; i < PokerogueContent::kSpeciesCount; ++i) {
        if (pokemonEvolutionTextEqual(PokerogueContent::kSpecies[i].id, speciesId)) {
            return &PokerogueContent::kSpecies[i];
        }
    }
    return nullptr;
}

// Checks if the given species qualifies for a level-based evolution when transitioning
// from oldLevel to newLevel. Returns pointer to canonical SpeciesEvolution or nullptr.
inline const PokerogueContent::SpeciesEvolution* checkSpeciesLevelEvolution(
    const char* currentSpeciesId, uint16_t oldLevel, uint16_t newLevel) {
    if (!currentSpeciesId || oldLevel >= newLevel) return nullptr;

    for (std::size_t i = 0; i < PokerogueContent::kSpeciesEvolutionCount; ++i) {
        const auto& edge = PokerogueContent::kSpeciesEvolutions[i];
        if (!pokemonEvolutionTextEqual(edge.sourceSpeciesId, currentSpeciesId)) continue;

        // Level-based evolution: edge.level > 1, newLevel >= edge.level, and oldLevel < edge.level
        if (edge.level > 1 && newLevel >= edge.level) {
            for (const auto& capability : PokerogueContent::kSimpleLevelEvolutionProfiles)
                if (capability.sourceOrder == edge.sourceOrder &&
                    pokemonEvolutionTextEqual(capability.speciesId, currentSpeciesId)) return &edge;
            // Conditional/item/form evolutions require their own resolved policy.
        }
    }
    return nullptr;
}

// Checks learnset moves for a species between oldLevel and newLevel, learning moves
// into unoccupied move slots (< 4). Returns count of moves newly learned.
inline uint8_t learnNewLevelMoves(
    uint16_t speciesDex, uint16_t oldLevel, uint16_t newLevel,
    PokemonBattleState& battleState, uint16_t* moveIdsOutput, uint8_t& moveCountOutput,
    std::string* feedback = nullptr) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species || oldLevel >= newLevel) return 0;

    const auto* levelMoves = PokerogueContent::levelMovesFor(*species);
    if (!levelMoves) return 0;

    uint8_t learnedCount = 0;
    for (uint16_t i = 0; i < species->learnsetCount; ++i) {
        const auto& lm = levelMoves[i];
        if (lm.level <= 0 || static_cast<uint16_t>(lm.level) <= oldLevel || static_cast<uint16_t>(lm.level) > newLevel) {
            continue;
        }

        // Check if move is already known
        bool alreadyKnown = false;
        for (uint8_t slot = 0; slot < battleState.moveCount; ++slot) {
            if (battleState.moves[slot].moveId == lm.moveId) {
                alreadyKnown = true;
                break;
            }
        }
        if (alreadyKnown) continue;

        // Learn move if slot available (< 4)
        if (battleState.moveCount < 4) {
            const auto* moveDef = PokerogueContent::findMoveById(lm.moveId);
            if (!moveDef) continue;

            const uint8_t slot = battleState.moveCount;
            battleState.moves[slot].moveId = lm.moveId;
            battleState.moves[slot].pp = moveDef->pp;
            battleState.moves[slot].maxPp = moveDef->pp;
            ++battleState.moveCount;

            if (moveIdsOutput && slot < 4) {
                moveIdsOutput[slot] = lm.moveId;
            }
            moveCountOutput = battleState.moveCount;
            ++learnedCount;

            if (feedback) {
                if (!feedback->empty()) *feedback += " ";
                *feedback += "Learned " + std::string(moveDef->name) + "!";
            }
        }
    }
    return learnedCount;
}

struct EvolutionResult {
    bool evolved = false;
    uint16_t newDex = 0;
    const char* newSpeciesId = nullptr;
    const char* newName = nullptr;
};

// Applies a level evolution to the Pokémon, re-calculating base stats and updating identity.
inline bool applySpeciesEvolution(
    uint16_t oldDex, const char* targetSpeciesId,
    PokemonBattleState& battleState, EvolutionResult& result,
    std::string* feedback = nullptr, PokemonActorIdentity* identity = nullptr) {
    const auto* target = findSpeciesById(targetSpeciesId);
    if (!target) return false;

    const auto* oldSpecies = PokerogueContent::findSpeciesByDex(oldDex);
    const char* oldName = oldSpecies ? oldSpecies->name : "Pokémon";

    PokemonBattleInit input{};
    input.speciesDex = target->dex;
    if (!oldSpecies || battleState.speciesDex != oldDex) return false;
    const auto* oldForm = battleState.formId ? PokerogueContent::findFormById(battleState.formId) : nullptr;
    if (battleState.formId && !oldForm) return false;
    input.formId = target->firstFormId && *target->firstFormId ? target->firstFormId : nullptr;
    const auto* targetForm = input.formId ? PokerogueContent::findFormById(input.formId) : nullptr;
    if (input.formId && !targetForm) return false;
    const uint16_t oldAbilities[] = {oldForm ? oldForm->ability1 : oldSpecies->ability1,
        oldForm ? oldForm->ability2 : oldSpecies->ability2,
        oldForm ? oldForm->abilityHidden : oldSpecies->abilityHidden};
    const uint16_t nextAbilities[] = {targetForm ? targetForm->ability1 : target->ability1,
        targetForm ? targetForm->ability2 : target->ability2,
        targetForm ? targetForm->abilityHidden : target->abilityHidden};
    uint8_t abilitySlot = identity ? identity->abilityIndex : 0;
    if (identity) {
        if (abilitySlot > 2 || identity->pokemonId != battleState.pokemonId ||
            identity->gender != battleState.gender || identity->nature != battleState.nature ||
            (oldAbilities[abilitySlot] ? oldAbilities[abilitySlot] : oldAbilities[0]) != battleState.abilityId)
            return false;
    } else {
        while (abilitySlot < 3 && oldAbilities[abilitySlot] != battleState.abilityId) ++abilitySlot;
        if (abilitySlot == 3) return false;
    }
    // PlayerPokemon.evolve: hidden slot changes to slot one when the target
    // has no hidden ability (getAbilityCount changes from three to two).
    if (abilitySlot == 2 && oldAbilities[2] && !nextAbilities[2]) abilitySlot = 1;
    const uint16_t evolvedAbility = nextAbilities[abilitySlot] ? nextAbilities[abilitySlot] : nextAbilities[0];
    input.level = battleState.level;
    input.pokemonId = battleState.pokemonId;
    input.deriveIvsFromPokemonId = battleState.ivsWereDerivedFromPokemonId;
    input.nature = battleState.nature;
    input.gender = battleState.gender;
    input.abilityId = evolvedAbility;
    input.moveCount = battleState.moveCount;
    for (uint8_t i = 0; i < 6; ++i) input.ivs[i] = battleState.ivs[i];
    for (uint8_t i = 0; i < battleState.moveCount; ++i) input.moveIds[i] = battleState.moves[i].moveId;

    PokemonBattleState evolvedState{};
    if (initializePokemonBattleState(input, evolvedState) != PokemonBattleInitResult::Ok) {
        return false;
    }

    // Preserve HP with max HP increase, and current PP and stat stages
    if (battleState.hp && evolvedState.maxHp > battleState.maxHp) {
        evolvedState.hp = static_cast<uint16_t>(battleState.hp + (evolvedState.maxHp - battleState.maxHp));
    } else {
        evolvedState.hp = std::min<uint16_t>(battleState.hp, evolvedState.maxHp);
    }
    for (uint8_t i = 0; i < battleState.moveCount; ++i) {
        evolvedState.moves[i].pp = battleState.moves[i].pp;
    }
    for (uint8_t stat = 0; stat < 7; ++stat) {
        evolvedState.statStages[stat] = battleState.statStages[stat];
    }

    battleState = evolvedState;
    if (identity) {
        identity->abilityIndex = abilitySlot;
        identity->formId = evolvedState.formId;
    }

    result.evolved = true;
    result.newDex = target->dex;
    result.newSpeciesId = target->id;
    result.newName = target->name;

    if (feedback) {
        if (!feedback->empty()) *feedback += " ";
        *feedback += std::string("What? ") + oldName + " evolved into " + target->name + "!";
    }
    return true;
}

} // namespace Pokerogue3DS
