#pragma once

#include "game/PokerogueRngAdapter.hpp"
#include "game/PokemonBattleState.hpp"
#include <cmath>
#include <cstring>
#include <cstdint>

namespace Pokerogue3DS {

// EnemyPokemon.getNextMove uses this score-dependent walk for SMART trainers.
// The input scores are in moveset order, after target scoring and any KO-only
// filtering. Stable sorting keeps upstream order when two scores are equal.
inline bool selectSmartTrainerMoveSlot(const double* scores, const uint8_t* slots,
                                       uint8_t count, PokerogueRngAdapter& battleRng,
                                       uint8_t& selectedSlot) {
    if (!scores || !slots || !count || count > 4) return false;
    uint8_t ordered[4]{};
    for (uint8_t i = 0; i < count; ++i) {
        if (!std::isfinite(scores[i])) return false;
        ordered[i] = i;
        uint8_t position = i;
        while (position && scores[ordered[position]] > scores[ordered[position - 1]]) {
            const uint8_t previous = ordered[position - 1];
            ordered[position - 1] = ordered[position];
            ordered[position] = previous;
            --position;
        }
    }
    uint8_t chosen = 0;
    while (chosen + 1 < count) {
        const double current = scores[ordered[chosen]];
        const double next = scores[ordered[chosen + 1]];
        const double ratio = next / current;
        // JavaScript's NaN >= 0 is false. Infinity is allowed when the
        // current score is zero and the following score is positive.
        if (!(ratio >= 0.0)) break;
        const int32_t threshold = ratio >= 2.0 ? 101
            : static_cast<int32_t>(std::floor(ratio * 50.0 + 0.5));
        if (battleRng.randSeedInt(100) >= threshold) break;
        ++chosen;
    }
    selectedSlot = slots[ordered[chosen]];
    return true;
}

// AttackMove.getTargetBenefitScore plus EnemyPokemon.getNextMove's enemy
// target sign, effectiveness and STAB. Only plain moves without attributes,
// conditions or multi-turn behavior may use this resolved baseline.
inline bool calculatePlainAttackAiScore(double effectiveness, uint32_t selectedStat,
                                        uint32_t otherStat, int16_t power,
                                        int16_t accuracy, bool stab, double& output, double userBenefit = 0.0) {
    if (!std::isfinite(effectiveness) || effectiveness < 0 || !selectedStat ||
        power <= 0 || accuracy < -1 || accuracy > 100 || !std::isfinite(userBenefit)) return false;
    double attack = (effectiveness - 1.0) * (effectiveness - 1.0) *
        (effectiveness < 1.0 ? -2.0 : 2.0);
    const double statRatio = static_cast<double>(otherStat) / selectedStat;
    if (statRatio <= 0.75) attack *= 2.0;
    else if (statRatio <= 0.875) attack *= 1.5;
    const double effectivePower = power * (accuracy == -1 ? 1.0 : accuracy / 100.0);
    attack += std::floor(effectivePower / 5.0);
    // The negative target benefit becomes positive for an opposing target.
    double score = (attack + userBenefit) * effectiveness * (stab ? 1.5 : 1.0);
    if (!score) score = -20.0;
    if (!std::isfinite(score)) return false;
    output = score;
    return true;
}

// EnemyPokemon.getNextMove restricts SMART/SMART_RANDOM to KO moves when
// any usable move's simulated damage reaches the opposing target's HP.
// Inputs must come from the resolved damage/conditions layer; no RNG is used.
inline bool filterEnemyKoMoveSlots(const uint8_t* slots, const uint32_t* damages,
                                   uint8_t count, uint16_t targetHp,
                                   uint8_t output[4], uint8_t& written) {
    written = 0;
    if (!slots || !damages || !output || !count || count > 4 || !targetHp) return false;
    bool hasKo = false;
    for (uint8_t i = 0; i < count; ++i) {
        if (slots[i] >= 4) return false;
        for (uint8_t prior = 0; prior < i; ++prior)
            if (slots[i] == slots[prior]) return false;
        hasKo |= damages[i] >= targetHp;
    }
    for (uint8_t i = 0; i < count; ++i)
        if (!hasKo || damages[i] >= targetHp) output[written++] = slots[i];
    return true;
}

// Resolved inputs to Pokemon.getMatchupScore. The effect/type layer must
// account for abilities, illusion, effective speed and usable damaging moves.
// Attack effectiveness entries already include the source's conditional STAB.
struct PokemonTrainerMatchupInput {
    double defensiveEffectiveness[2]{1.0, 1.0};
    uint8_t opponentTypeCount = 1;
    double attackEffectiveness[4]{};
    uint8_t usableAttackCount = 0;
    double hpRatio = 1.0;
    double opponentHpRatio = 1.0;
    bool outspeeds = false;
    bool active = false;
};

// Canonical-state bridge for the neutral field baseline. Resolved speed is
// explicit so effective active speed and unmodified reserve speed can differ.
// Type-changing abilities/illusion/field effects require the full effect layer.
inline bool buildBaselineTrainerMatchupInput(const PokemonBattleState& actor,
    const PokemonBattleState& opponent, uint32_t actorSpeed, uint32_t opponentSpeed,
    bool active, PokemonTrainerMatchupInput& output) {
    if (!actor.maxHp || !opponent.maxHp || actor.hp > actor.maxHp ||
        opponent.hp > opponent.maxHp || actor.moveCount > 4) return false;
    const auto* species = PokerogueContent::findSpeciesByDex(actor.speciesDex);
    const auto* other = PokerogueContent::findSpeciesByDex(opponent.speciesDex);
    const auto* form = actor.formId ? PokerogueContent::findFormById(actor.formId) : nullptr;
    const auto* otherForm = opponent.formId ? PokerogueContent::findFormById(opponent.formId) : nullptr;
    if (!species || !other || (actor.formId && !form) || (opponent.formId && !otherForm))
        return false;
    const auto sameType = [](const char* a, const char* b) {
        if (!a || !b) return false;
        while (*a && *b) {
            char left = *a++, right = *b++;
            if (left >= 'a' && left <= 'z') left = static_cast<char>(left - 'a' + 'A');
            if (right >= 'a' && right <= 'z') right = static_cast<char>(right - 'a' + 'A');
            if (left != right) return false;
        }
        return *a == *b;
    };
    const char* actorTypes[2] = {form ? form->type1 : species->type1,
        form ? form->type2 : species->type2};
    const char* opponentTypes[2] = {otherForm ? otherForm->type1 : other->type1,
        otherForm ? otherForm->type2 : other->type2};
    if (!actorTypes[0] || !opponentTypes[0]) return false;
    PokemonTrainerMatchupInput next{};
    next.opponentTypeCount = opponentTypes[1] && *opponentTypes[1] &&
        !sameType(opponentTypes[1], "NONE") ? 2 : 1;
    for (uint8_t type = 0; type < next.opponentTypeCount; ++type)
        if (calculatePokemonAttackTypeEffectiveness(opponentTypes[type], actor,
                next.defensiveEffectiveness[type]) != PokemonTypeEffectivenessResult::Ok) return false;
    for (uint8_t slot = 0; slot < actor.moveCount; ++slot) {
        const auto* move = PokerogueContent::findMoveById(actor.moves[slot].moveId);
        if (!move) return false;
        if (!actor.moves[slot].pp || move->category == PokerogueContent::MoveStatus) continue;
        if (move->upstreamFlags & PokerogueContent::MoveHasVariableMovegenType) return false;
        double effectiveness = 0;
        if (calculatePokemonAttackTypeEffectiveness(move->type, opponent, effectiveness) !=
            PokemonTypeEffectivenessResult::Ok) return false;
        if (sameType(move->type, actorTypes[0]) || sameType(move->type, actorTypes[1]))
            effectiveness *= 1.5;
        next.attackEffectiveness[next.usableAttackCount++] = effectiveness;
    }
    next.hpRatio = static_cast<double>(actor.hp) / actor.maxHp;
    next.opponentHpRatio = static_cast<double>(opponent.hp) / opponent.maxHp;
    next.active = active;
    next.outspeeds = actorSpeed >= opponentSpeed;
    output = next;
    return true;
}

inline bool calculateTrainerMatchupScore(const PokemonTrainerMatchupInput& input,
                                         double& output) {
    if (!input.opponentTypeCount || input.opponentTypeCount > 2 ||
        input.usableAttackCount > 4 || !std::isfinite(input.hpRatio) ||
        !std::isfinite(input.opponentHpRatio) || input.hpRatio < 0 || input.hpRatio > 1 ||
        input.opponentHpRatio < 0 || input.opponentHpRatio > 1) return false;
    double defense = 1.0;
    for (uint8_t i = 0; i < input.opponentTypeCount; ++i) {
        const double effectiveness = input.defensiveEffectiveness[i];
        if (!std::isfinite(effectiveness) || effectiveness < 0) return false;
        defense /= effectiveness > 0.25 ? effectiveness : 0.25;
    }
    double attack = 0;
    for (uint8_t i = 0; i < input.usableAttackCount; ++i) {
        if (!std::isfinite(input.attackEffectiveness[i]) || input.attackEffectiveness[i] < 0)
            return false;
        attack += input.attackEffectiveness[i];
    }
    attack /= input.usableAttackCount ? input.usableAttackCount : 1;
    double hpDifference = input.hpRatio + (1.0 - input.opponentHpRatio);
    if (input.hpRatio <= 0.2 && input.active) {
        if (!input.outspeeds && attack < 1.5 && defense < 1.5)
            hpDifference *= 0.85;
        else hpDifference = 1.0 - input.hpRatio + (input.outspeeds ? 0.2 : 0.1);
    } else if (input.outspeeds) hpDifference *= 1.25;
    else if (input.hpRatio > 0.2 && input.hpRatio <= 0.4) hpDifference *= 0.5;
    const double score = (attack + defense) * (hpDifference < 1.0 ? hpDifference : 1.0);
    if (!std::isfinite(score)) return false;
    output = score;
    return true;
}

// EnemyCommandPhase's threshold after party eligibility, entry hazards and
// opponent averaging have been resolved by the caller. Traps/queued moves
// are command-layer gates and must be checked before invoking this function.
inline bool shouldTrainerSwitch(double activeScore, double bestReserveScore,
                                uint32_t switchCounter, bool boss, bool& output) {
    if (!std::isfinite(activeScore) || !std::isfinite(bestReserveScore) ||
        activeScore < 0 || bestReserveScore < 0) return false;
    const double multiplier = switchCounter
        ? 1.0 - std::pow(0.1, 1.0 / switchCounter) : 1.0;
    output = bestReserveScore * multiplier >= activeScore * (boss ? 2.0 : 3.0);
    return true;
}

// Trainer.getNextSummonIndex: retain party order among equally best scores.
// The caller scopes this RNG to waveSeed + (turn << 2); the battle-turn stream
// must not be passed here. A unique best member does not consume a draw.
inline bool selectTrainerSummonIndex(const double* scores, const uint8_t* indexes,
                                     uint8_t count, PokerogueRngAdapter& scopedRng,
                                     uint8_t& output) {
    if (!scores || !indexes || !count || count > 6) return false;
    double best = -1.0;
    uint8_t choices[6]{};
    uint8_t choiceCount = 0;
    for (uint8_t i = 0; i < count; ++i) {
        if (!std::isfinite(scores[i]) || scores[i] < 0 || indexes[i] >= 6) return false;
        for (uint8_t earlier = 0; earlier < i; ++earlier)
            if (indexes[earlier] == indexes[i]) return false;
        if (scores[i] > best) { best = scores[i]; choiceCount = 0; }
        if (scores[i] == best) choices[choiceCount++] = indexes[i];
    }
    output = choices[choiceCount > 1 ? scopedRng.randSeedInt(choiceCount) : 0];
    return true;
}

// Single-opponent replacement selection after fainting. The caller resolves
// field/ability effects and scopes RNG before invoking this baseline bridge.
// Fainted members and the current slot are excluded before matchup scoring.
inline bool selectBaselineTrainerReplacement(
    const PokemonBattleState* party, uint8_t count, uint8_t activeIndex,
    const PokemonBattleState& opponent, PokerogueRngAdapter& scopedRng,
    uint8_t& output) {
    if (!party || !count || count > 6 || activeIndex >= count ||
        !opponent.hp) return false;
    const auto* opponentSpecies = PokerogueContent::findSpeciesByDex(opponent.speciesDex);
    if (!opponentSpecies || opponentSpecies->legendary < 0) return false;
    double scores[6]{};
    uint8_t indexes[6]{};
    uint8_t eligible = 0;
    for (uint8_t member = 0; member < count; ++member) {
        if (member == activeIndex || !party[member].hp) continue;
        PokemonTrainerMatchupInput input{};
        double score = 0;
        uint32_t actorSpeed = 0, opponentSpeed = 0;
        if (!pokemonBaselineEffectiveStat(party[member], 5, false, actorSpeed) ||
            !pokemonBaselineEffectiveStat(opponent, 5, false, opponentSpeed) ||
            !buildBaselineTrainerMatchupInput(party[member], opponent,
                actorSpeed, opponentSpeed, false, input) ||
            !calculateTrainerMatchupScore(input, score)) return false;
        scores[eligible] = opponentSpecies->legendary ? score / 2.0 : score;
        indexes[eligible++] = member;
    }
    if (!eligible) return false;
    return selectTrainerSummonIndex(scores, indexes, eligible, scopedRng, output);
}

struct TrainerSwitchDecision {
    bool switchPokemon = false;
    uint8_t partyIndex = 0xFF;
    uint32_t nextSwitchCounter = 0;
};

// EnemyCommandPhase's decision after the caller resolves trap/queue state and
// reserve scores including entry hazards. A move command decays the counter;
// a switch increments it. Reserve selection uses the separately scoped RNG.
inline bool resolveTrainerSwitchDecision(
    double activeScore, const double* reserveScores, const uint8_t* reserveIndexes,
    uint8_t reserveCount, uint32_t switchCounter, bool boss, bool trapped,
    bool hasQueuedMove, PokerogueRngAdapter& scopedRng, TrainerSwitchDecision& output) {
    TrainerSwitchDecision next{};
    next.nextSwitchCounter = switchCounter ? switchCounter - 1 : 0;
    if (!std::isfinite(activeScore) || activeScore < 0 || reserveCount > 6) return false;
    if (!trapped && !hasQueuedMove && reserveCount) {
        if (!reserveScores || !reserveIndexes) return false;
        double best = 0;
        for (uint8_t member = 0; member < reserveCount; ++member) {
            if (!std::isfinite(reserveScores[member]) || reserveScores[member] < 0 ||
                reserveIndexes[member] >= 6) return false;
            for (uint8_t earlier = 0; earlier < member; ++earlier)
                if (reserveIndexes[earlier] == reserveIndexes[member]) return false;
            if (reserveScores[member] > best) best = reserveScores[member];
        }
        bool shouldSwitch = false;
        if (!shouldTrainerSwitch(activeScore, best, switchCounter, boss, shouldSwitch)) return false;
        if (shouldSwitch) {
            if (switchCounter == 0xFFFFFFFFu ||
                !selectTrainerSummonIndex(reserveScores, reserveIndexes, reserveCount,
                    scopedRng, next.partyIndex)) return false;
            next.switchPokemon = true;
            next.nextSwitchCounter = switchCounter + 1;
        }
    }
    output = next;
    return true;
}

// Raw StatStageChangeAttr.getTargetBenefitScore. Target allegiance and other
// move/ability benefits are combined by the command scorer, not this helper.
inline bool calculateStatStageTargetBenefit(
    const PokemonBattleState& user, const PokemonBattleState& target,
    const PokerogueContent::MoveStatStageEffect& effect, double& output) {
    if (!effect.statMask || effect.statMask > 127 || effect.stages < -6 ||
        effect.stages > 6 || user.moveCount > 4) return false;
    bool hasPhysicalAttack = false;
    for (uint8_t slot = 0; slot < user.moveCount; ++slot) {
        const auto* move = PokerogueContent::findMoveById(user.moves[slot].moveId);
        if (!move) return false;
        if (move->category == PokerogueContent::MovePhysical) hasPhysicalAttack = true;
    }
    double score = 0;
    for (uint8_t stat = 1; stat <= 7; ++stat) {
        if (!(effect.statMask & (1u << (stat - 1)))) continue;
        const int current = target.statStages[stat - 1];
        if (current < -6 || current > 6) return false;
        int next = current + effect.stages;
        if (next > 6) next = 6;
        if (next < -6) next = -6;
        const int levels = next - current;
        // The pinned source uses PHYSICAL for SPATK/SPDEF too. Preserve this
        // source behavior instead of substituting an intended-looking formula.
        const bool needsPhysical = ((stat == 1 || stat == 3) && effect.selfTarget) ||
            ((stat == 2 || stat == 4) && !effect.selfTarget);
        if (needsPhysical && !hasPhysicalAttack) continue;
        score += levels * 4 + (levels > 0 ? -2 : 2);
    }
    output = score;
    return true;
}

inline bool calculateCanonicalStatStageStatusAiScore(
    const PokemonBattleState& user, const PokemonBattleState& opponent,
    uint16_t moveId, double& output) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move || move->category != PokerogueContent::MoveStatus || move->attributeCount != 1 ||
        !PokerogueContent::moveHasAttribute(*move, "StatStageChangeAttr") || !move->target)
        return false;
    const bool selfTarget = std::strcmp(move->target, "USER") == 0;
    if (!selfTarget && std::strcmp(move->target, "NEAR_ENEMY") != 0 &&
        std::strcmp(move->target, "ALL_NEAR_ENEMIES") != 0) return false;
    const PokerogueContent::MoveStatStageEffect* effect = nullptr;
    for (const auto& entry : PokerogueContent::kMoveStatStageEffects)
        if (entry.moveId == moveId) {
            if (effect) return false;
            effect = &entry;
        }
    if (!effect) return false;
    double targetBenefit = 0;
    if (!calculateStatStageTargetBenefit(user, selfTarget ? user : opponent,
            *effect, targetBenefit)) return false;
    // EnemyPokemon.getNextMove: opposing target benefit is negated; own
    // benefit is positive. Status scores do not receive STAB/type multipliers.
    output = targetBenefit * (selfTarget ? 1.0 : -1.0);
    return true;
}

} // namespace Pokerogue3DS
