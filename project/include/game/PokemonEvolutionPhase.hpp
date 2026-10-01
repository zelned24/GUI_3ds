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

enum class PokemonLearnMoveResult : uint8_t {
    Learned = 0, AlreadyKnown, InvalidSlot, MissingMove, UpstreamUnimplemented, InvalidState
};
// LearnMovePhase.learnMove -> Pokemon.setMove. UI owns replace/reject choice;
// this operation replaces exactly its selected slot and starts with full PP.
inline PokemonLearnMoveResult learnPokemonMoveAtSlot(PokemonBattleState& state,
    uint16_t moveId, uint8_t selectedSlot) {
    if (state.moveCount > 4) return PokemonLearnMoveResult::InvalidState;
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || !moveId) return PokemonLearnMoveResult::MissingMove;
    if (move->upstreamFlags & PokerogueContent::MoveIsUnimplemented)
        return PokemonLearnMoveResult::UpstreamUnimplemented;
    if (move->pp < 1 || move->pp > 255) return PokemonLearnMoveResult::InvalidState;
    if (selectedSlot >= 4 || selectedSlot > state.moveCount) return PokemonLearnMoveResult::InvalidSlot;
    for (uint8_t slot = 0; slot < state.moveCount; ++slot)
        if (state.moves[slot].moveId == moveId) return PokemonLearnMoveResult::AlreadyKnown;
    state.moves[selectedSlot] = {moveId, static_cast<uint8_t>(move->pp), static_cast<uint8_t>(move->pp)};
    if (selectedSlot == state.moveCount) ++state.moveCount;
    return PokemonLearnMoveResult::Learned;
}

struct PokemonPendingLevelMoves {
    uint16_t moveIds[128]{};
    uint16_t count = 0;
    bool overflow = false;
};

// EvolutionPhase.postEvolve requests EVOLVE_MOVE (pinned constants.ts: 0).
inline bool learnPokemonEvolutionMoves(PokemonBattleState& state, PokemonPendingLevelMoves& pending) {
    const auto* species = PokerogueContent::findSpeciesByDex(state.speciesDex);
    const auto* form = state.formId ? PokerogueContent::findFormById(state.formId) : nullptr;
    if (!species || state.moveCount > 4 || pending.count > 128 ||
        (state.formId && (!form || !pokemonEvolutionTextEqual(form->speciesId, species->id)))) return false;
    auto next = state;
    auto queue = pending;
    for (uint8_t source = 0; source < 2; ++source) {
        const auto* rows = source ? (form ? PokerogueContent::levelMovesFor(*form) : nullptr)
            : PokerogueContent::levelMovesFor(*species);
        const uint16_t count = source ? (form ? form->learnsetCount : 0) : species->learnsetCount;
        for (uint16_t i = 0; rows && i < count; ++i) {
            if (rows[i].level != 0) continue;
            const auto* move = PokerogueContent::findMoveById(rows[i].moveId);
            if (!move) return false;
            if (move->upstreamFlags & PokerogueContent::MoveIsUnimplemented) continue;
            bool known = false;
            for (uint8_t slot = 0; slot < next.moveCount; ++slot) known |= next.moves[slot].moveId == move->id;
            for (uint16_t q = 0; q < queue.count; ++q) known |= queue.moveIds[q] == move->id;
            if (known) continue;
            if (next.moveCount < 4) {
                if (learnPokemonMoveAtSlot(next, move->id, next.moveCount) != PokemonLearnMoveResult::Learned)
                    return false;
            } else {
                if (queue.count == 128) return false;
                queue.moveIds[queue.count++] = move->id;
            }
        }
    }
    state = next;
    pending = queue;
    return true;
}

// Checks learnset moves for a species between oldLevel and newLevel, learning moves
// into unoccupied move slots (< 4). Returns count of moves newly learned.
inline uint8_t learnNewLevelMoves(
    uint16_t speciesDex, uint16_t oldLevel, uint16_t newLevel,
    PokemonBattleState& battleState, uint16_t* moveIdsOutput, uint8_t& moveCountOutput,
    std::string* feedback = nullptr, PokemonPendingLevelMoves* pending = nullptr) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species || oldLevel >= newLevel) return 0;

    if (battleState.speciesDex != speciesDex || battleState.moveCount > 4) return 0;
    const auto* form = battleState.formId ? PokerogueContent::findFormById(battleState.formId) : nullptr;
    if (battleState.formId && (!form || !pokemonEvolutionTextEqual(form->speciesId, species->id))) return 0;
    const auto* speciesMoves = PokerogueContent::levelMovesFor(*species);
    const auto* formMoves = form ? PokerogueContent::levelMovesFor(*form) : nullptr;
    const uint16_t speciesCount = species->learnsetCount;
    const uint16_t formCount = form ? form->learnsetCount : 0;
    if ((!speciesMoves && speciesCount) || (!formMoves && formCount)) return 0;
    // SpeciesDataRegistry merges species + form moves; Pokemon.getLevelMoves
    // stable-sorts by level. Select ascending levels without allocating a pool.
    uint16_t previousLevel = oldLevel;
    uint8_t learnedCount = 0;
    while (previousLevel < newLevel) {
        uint16_t selectedLevel = 0;
        for (uint8_t source = 0; source < 2; ++source) {
            const auto* rows = source ? formMoves : speciesMoves;
            const uint16_t count = source ? formCount : speciesCount;
            for (uint16_t i = 0; rows && i < count; ++i)
                if (rows[i].level > 0 && static_cast<uint16_t>(rows[i].level) > previousLevel &&
                    static_cast<uint16_t>(rows[i].level) <= newLevel &&
                    (!selectedLevel || rows[i].level < selectedLevel)) selectedLevel = rows[i].level;
        }
        if (!selectedLevel) break;
        previousLevel = selectedLevel;
        for (uint8_t source = 0; source < 2; ++source) {
            const auto* levelMoves = source ? formMoves : speciesMoves;
            const uint16_t count = source ? formCount : speciesCount;
            for (uint16_t i = 0; levelMoves && i < count; ++i) {
                const auto& lm = levelMoves[i];
                if (lm.level != selectedLevel) continue;

                // Check if move is already known
                bool alreadyKnown = false;
                for (uint8_t slot = 0; slot < battleState.moveCount; ++slot) {
                    if (battleState.moves[slot].moveId == lm.moveId) {
                        alreadyKnown = true;
                        break;
                    }
                }
                if (alreadyKnown) continue;
                if (battleState.moveCount == 4 && pending) {
                    const auto* candidate = PokerogueContent::findMoveById(lm.moveId);
                    if (!candidate || (candidate->upstreamFlags & PokerogueContent::MoveIsUnimplemented)) continue;
                    bool queued = false;
                    for (uint16_t q = 0; q < pending->count; ++q) queued |= pending->moveIds[q] == lm.moveId;
                    if (!queued) {
                        if (pending->count == 128) pending->overflow = true;
                        else pending->moveIds[pending->count++] = lm.moveId;
                    }
                }

                // Learn move if slot available (< 4)
                if (battleState.moveCount < 4) {
                    const auto* moveDef = PokerogueContent::findMoveById(lm.moveId);
                    if (!moveDef) continue;

                    const uint8_t slot = battleState.moveCount;
                    if (learnPokemonMoveAtSlot(battleState, lm.moveId, slot) != PokemonLearnMoveResult::Learned)
                        continue;

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
    const char* originalTeraType = identity ? identity->initialTeraType : nullptr;
    if (identity && identity->initialTeraTypeResolved) {
        if (identity->initialTeraTypeIndex > 1) return false;
        originalTeraType = resolvePokemonTypeSymbol(originalTeraType ? originalTeraType
            : identity->initialTeraTypeIndex ? (oldForm ? oldForm->type2 : oldSpecies->type2)
                : (oldForm ? oldForm->type1 : oldSpecies->type1));
        if (!originalTeraType) return false;
    }
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

    evolvedState.friendship = battleState.friendship;
    evolvedState.pauseEvolutions = battleState.pauseEvolutions;
    battleState = evolvedState;
    if (identity) {
        identity->initialTeraType = originalTeraType;
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
