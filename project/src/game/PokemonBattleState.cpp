#include "game/PokemonBattleState.hpp"
#include "game/PokerogueRngAdapter.hpp"

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

bool typeInList(const char* type, const char* list) {
    if (!type || !list || !*type) return false;
    std::size_t typeLength = 0;
    while (type[typeLength]) ++typeLength;
    const char* item = list;
    while (*item) {
        while (*item == '|') ++item;
        const char* end = item;
        while (*end && *end != '|') ++end;
        if (static_cast<std::size_t>(end - item) == typeLength) {
            std::size_t index = 0;
            while (index < typeLength) {
                const char value = type[index] >= 'a' && type[index] <= 'z'
                    ? static_cast<char>(type[index] - 'a' + 'A') : type[index];
                if (item[index] != value) break;
                ++index;
            }
            if (index == typeLength) return true;
        }
        item = end;
    }
    return false;
}

struct TypeChartRow { const char* defendingType; const char* weakTo; const char* resists; const char* immuneTo; };
// Extracted from the pinned PokemonType chart in src/data/type.ts.
const TypeChartRow kTypeChart[] = {
    {"NORMAL", "|FIGHTING|", "||", "|GHOST|"},
    {"FIGHTING", "|FLYING|PSYCHIC|FAIRY|", "|ROCK|BUG|DARK|", "||"},
    {"FLYING", "|ROCK|ELECTRIC|ICE|", "|FIGHTING|BUG|GRASS|", "|GROUND|"},
    {"POISON", "|GROUND|PSYCHIC|", "|FIGHTING|POISON|BUG|GRASS|FAIRY|", "||"},
    {"GROUND", "|WATER|GRASS|ICE|", "|POISON|ROCK|", "|ELECTRIC|"},
    {"ROCK", "|FIGHTING|GROUND|STEEL|WATER|GRASS|", "|NORMAL|FLYING|POISON|FIRE|", "||"},
    {"BUG", "|FLYING|ROCK|FIRE|", "|FIGHTING|GROUND|GRASS|", "||"},
    {"GHOST", "|GHOST|DARK|", "|POISON|BUG|", "|NORMAL|FIGHTING|"},
    {"STEEL", "|FIGHTING|GROUND|FIRE|", "|NORMAL|FLYING|ROCK|BUG|STEEL|GRASS|PSYCHIC|ICE|DRAGON|FAIRY|", "|POISON|"},
    {"FIRE", "|GROUND|ROCK|WATER|", "|BUG|STEEL|FIRE|GRASS|ICE|FAIRY|", "||"},
    {"WATER", "|GRASS|ELECTRIC|", "|STEEL|FIRE|WATER|ICE|", "||"},
    {"GRASS", "|FLYING|POISON|BUG|FIRE|ICE|", "|GROUND|WATER|GRASS|ELECTRIC|", "||"},
    {"ELECTRIC", "|GROUND|", "|FLYING|STEEL|ELECTRIC|", "||"},
    {"PSYCHIC", "|BUG|GHOST|DARK|", "|FIGHTING|PSYCHIC|", "||"},
    {"ICE", "|FIGHTING|ROCK|STEEL|FIRE|", "|ICE|", "||"},
    {"DRAGON", "|ICE|DRAGON|FAIRY|", "|FIRE|WATER|GRASS|ELECTRIC|", "||"},
    {"DARK", "|FIGHTING|BUG|FAIRY|", "|GHOST|DARK|", "|PSYCHIC|"},
    {"FAIRY", "|POISON|STEEL|", "|FIGHTING|BUG|DARK|", "|DRAGON|"},
};

bool sameText(const char* left, const char* right) {
    if (!left || !right) return false;
    while (*left && *right) {
        const char leftValue = *left >= 'a' && *left <= 'z' ? static_cast<char>(*left - 'a' + 'A') : *left;
        const char rightValue = *right >= 'a' && *right <= 'z' ? static_cast<char>(*right - 'a' + 'A') : *right;
        if (leftValue != rightValue) return false;
        ++left;
        ++right;
    }
    return *left == *right;
}

bool oneTypeEffectiveness(const char* attack, const char* defend, double& multiplier) {
    if (sameText(attack, "UNKNOWN") || sameText(defend, "UNKNOWN") || sameText(attack, "STELLAR") || sameText(defend, "STELLAR")) {
        multiplier = 1.0;
        return true;
    }
    for (const auto& row : kTypeChart) {
        if (!sameText(row.defendingType, defend)) continue;
        if (typeInList(attack, row.immuneTo)) multiplier = 0.0;
        else if (typeInList(attack, row.weakTo)) multiplier = 2.0;
        else if (typeInList(attack, row.resists)) multiplier = 0.5;
        else multiplier = 1.0;
        return true;
    }
    return false;
}
}

void derivePokemonIvsFromId(uint32_t pokemonId, uint8_t outputIvs[6]) {
    if (!outputIvs) return;
    outputIvs[0] = static_cast<uint8_t>((pokemonId & 0x3E000000u) >> 25);
    outputIvs[1] = static_cast<uint8_t>((pokemonId & 0x01F00000u) >> 20);
    outputIvs[2] = static_cast<uint8_t>((pokemonId & 0x000F8000u) >> 15);
    outputIvs[3] = static_cast<uint8_t>((pokemonId & 0x00007C00u) >> 10);
    outputIvs[4] = static_cast<uint8_t>((pokemonId & 0x000003E0u) >> 5);
    outputIvs[5] = static_cast<uint8_t>(pokemonId & 0x0000001Fu);
}

PokemonActorIdentityResult generatePokemonActorIdentity(
    uint16_t speciesDex,
    uint16_t hiddenAbilityRate,
    PokerogueRngAdapter& rng,
    PokemonActorIdentity& output) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonActorIdentityResult::MissingSpecies;
    if (hiddenAbilityRate == 0) return PokemonActorIdentityResult::InvalidHiddenRate;

    PokemonActorIdentity next{};
    // Pokemon.generateAbilityIndex runs before assigning this.id.
    const auto abilityResult = selectPokemonAbilityIndex(
        speciesDex, hiddenAbilityRate, rng, next.abilityIndex);
    if (abilityResult != PokemonAbilitySelectionResult::Ok) {
        return PokemonActorIdentityResult::InvalidHiddenRate;
    }
    next.pokemonId = rng.randSeedUint32();
    derivePokemonIvsFromId(next.pokemonId, next.ivs);
    const auto genderResult = selectPokemonGender(speciesDex, rng, next.gender);
    if (genderResult == PokemonGenderSelectionResult::MissingSpecies) {
        return PokemonActorIdentityResult::MissingSpecies;
    }
    if (genderResult != PokemonGenderSelectionResult::Ok) {
        return PokemonActorIdentityResult::InvalidGenderRatio;
    }
    output = next;
    return PokemonActorIdentityResult::Ok;
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
    const bool speciesIsGenderless = species->malePercentTenths == 65534;
    if ((speciesIsGenderless && input.gender != PokemonGender::Genderless) ||
        (!speciesIsGenderless && (species->malePercentTenths > 1000 ||
            (input.gender != PokemonGender::Male && input.gender != PokemonGender::Female)))) {
        return PokemonBattleInitResult::InvalidGender;
    }
    uint8_t instanceIvs[6];
    if (input.deriveIvsFromPokemonId) derivePokemonIvsFromId(input.pokemonId, instanceIvs);
    else for (uint8_t i = 0; i < 6; ++i) instanceIvs[i] = input.ivs[i];
    for (uint8_t iv : instanceIvs) if (iv > 31) return PokemonBattleInitResult::InvalidIv;
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
    next.pokemonId = input.pokemonId;
    next.abilityId = input.abilityId;
    next.gender = input.gender;
    for (uint8_t i = 0; i < 6; ++i) next.ivs[i] = instanceIvs[i];
    next.ivsWereDerivedFromPokemonId = input.deriveIvsFromPokemonId;
    const uint8_t baseStats[6] = {form ? form->hp : species->hp, form ? form->atk : species->atk,
        form ? form->def : species->def, form ? form->spatk : species->spatk,
        form ? form->spdef : species->spdef, form ? form->speed : species->speed};
    for (uint8_t index = 0; index < 6; ++index) {
        next.stats[index] = calculatedStat(baseStats[index], instanceIvs[index], input.level,
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

PokemonBaseDamageResult calculatePokemonBaseDamage(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    double& outputBaseDamage) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move) return PokemonBaseDamageResult::MissingMove;
    if (move->category == PokerogueContent::MoveStatus || move->power <= 0) {
        return PokemonBaseDamageResult::NonDamagingMove;
    }

    const bool physical = move->category == PokerogueContent::MovePhysical;
    const uint16_t attack = attacker.stats[physical ? 1 : 3];
    const uint16_t defense = defender.stats[physical ? 2 : 4];
    if (attacker.level == 0 || defense == 0 || attack == 0) return PokemonBaseDamageResult::InvalidStats;

    // Pinned Pokemon.getBaseDamage formula before modifiers (STAB, type,
    // weather, random factor, abilities, items and move attributes).
    const double levelMultiplier = (2.0 * attacker.level) / 5.0 + 2.0;
    const double baseDamage = (levelMultiplier * move->power * attack) / defense / 50.0 + 2.0;
    outputBaseDamage = baseDamage;
    return PokemonBaseDamageResult::Ok;
}

PokemonTypeEffectivenessResult calculatePokemonTypeEffectiveness(
    uint16_t moveId,
    const PokemonBattleState& defender,
    double& outputMultiplier) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move) return PokemonTypeEffectivenessResult::MissingMove;
    const auto* species = PokerogueContent::findSpeciesByDex(defender.speciesDex);
    if (!species) return PokemonTypeEffectivenessResult::MissingSpecies;
    const auto* form = defender.formId ? PokerogueContent::findFormById(defender.formId) : nullptr;
    if (defender.formId && !form) return PokemonTypeEffectivenessResult::InvalidType;

    const char* type1 = form ? form->type1 : species->type1;
    const char* type2 = form ? form->type2 : species->type2;
    double first = 1.0;
    double second = 1.0;
    const bool hasSecondType = type2 && *type2 && !sameText(type2, "NONE");
    if (!oneTypeEffectiveness(move->type, type1, first) ||
        (hasSecondType && !oneTypeEffectiveness(move->type, type2, second))) {
        return PokemonTypeEffectivenessResult::InvalidType;
    }
    outputMultiplier = first * second;
    return PokemonTypeEffectivenessResult::Ok;
}

PokemonDamageCoreResult calculatePokemonDamageCore(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    bool moveIsTypeless,
    uint32_t& outputDamage) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move) return PokemonDamageCoreResult::MissingMove;
    if (move->category == PokerogueContent::MoveStatus || move->power <= 0) {
        return PokemonDamageCoreResult::NonDamagingMove;
    }
    const auto* attackerSpecies = PokerogueContent::findSpeciesByDex(attacker.speciesDex);
    const auto* defenderSpecies = PokerogueContent::findSpeciesByDex(defender.speciesDex);
    if (!attackerSpecies || !defenderSpecies) return PokemonDamageCoreResult::MissingSpecies;
    const auto* attackerForm = attacker.formId ? PokerogueContent::findFormById(attacker.formId) : nullptr;
    if (attacker.formId && !attackerForm) return PokemonDamageCoreResult::InvalidType;

    double baseDamage = 0.0;
    if (calculatePokemonBaseDamage(attacker, defender, moveId, baseDamage) != PokemonBaseDamageResult::Ok) {
        return PokemonDamageCoreResult::InvalidStats;
    }
    double typeMultiplier = 1.0;
    if (calculatePokemonTypeEffectiveness(moveId, defender, typeMultiplier) != PokemonTypeEffectivenessResult::Ok) {
        return PokemonDamageCoreResult::InvalidType;
    }
    if (typeMultiplier == 0.0) {
        outputDamage = 0;
        return PokemonDamageCoreResult::Ok;
    }

    double stabMultiplier = 1.0;
    if (!moveIsTypeless && !sameText(move->type, "STELLAR")) {
        const char* attackerType1 = attackerForm ? attackerForm->type1 : attackerSpecies->type1;
        const char* attackerType2 = attackerForm ? attackerForm->type2 : attackerSpecies->type2;
        if (sameText(move->type, attackerType1) ||
            (attackerType2 && *attackerType2 && !sameText(attackerType2, "NONE") &&
                sameText(move->type, attackerType2))) {
            stabMultiplier = 1.5;
        }
    }

    // Mirrors the pinned deterministic/simulated core: critical=1, random=1,
    // weather/field/status/ability/item modifiers=1; toDmgValue floors with min 1.
    const double adjusted = baseDamage * stabMultiplier * typeMultiplier;
    if (adjusted > 4294967295.0) return PokemonDamageCoreResult::InvalidStats;
    const uint32_t rounded = static_cast<uint32_t>(adjusted);
    outputDamage = rounded ? rounded : 1;
    return PokemonDamageCoreResult::Ok;
}

PokemonMoveDamageResult resolveStandardPokemonMoveDamage(
    const PokemonBattleState& attacker,
    const PokemonBattleState& defender,
    uint16_t moveId,
    bool moveIsTypeless,
    PokerogueRngAdapter& battleRng,
    PokemonMoveDamageRoll& output) {
    const auto* move = PokerogueContent::findMoveById(moveId);
    if (!move) return PokemonMoveDamageResult::MissingMove;
    if (move->category == PokerogueContent::MoveStatus || move->power <= 0) {
        return PokemonMoveDamageResult::NonDamagingMove;
    }
    if (move->accuracy < -1 || move->accuracy > 100) return PokemonMoveDamageResult::InvalidAccuracy;
    const auto* attackerSpecies = PokerogueContent::findSpeciesByDex(attacker.speciesDex);
    const auto* defenderSpecies = PokerogueContent::findSpeciesByDex(defender.speciesDex);
    if (!attackerSpecies || !defenderSpecies) return PokemonMoveDamageResult::MissingSpecies;
    const auto* attackerForm = attacker.formId ? PokerogueContent::findFormById(attacker.formId) : nullptr;
    if (attacker.formId && !attackerForm) return PokemonMoveDamageResult::InvalidType;

    PokemonMoveDamageRoll next{};
    if (calculatePokemonTypeEffectiveness(moveId, defender, next.typeEffectiveness) !=
        PokemonTypeEffectivenessResult::Ok) return PokemonMoveDamageResult::InvalidType;
    // Upstream checks type immunity before accuracy, critical and damage RNG.
    if (next.typeEffectiveness == 0.0) {
        output = next;
        return PokemonMoveDamageResult::Ok;
    }

    double baseDamage = 0.0;
    if (calculatePokemonBaseDamage(attacker, defender, moveId, baseDamage) != PokemonBaseDamageResult::Ok) {
        return PokemonMoveDamageResult::InvalidStats;
    }

    if (move->accuracy >= 0) {
        next.accuracyWasRolled = true;
        next.accuracyRoll = static_cast<uint8_t>(battleRng.randSeedInt(100));
        if (next.accuracyRoll >= move->accuracy) {
            output = next;
            return PokemonMoveDamageResult::Ok;
        }
    }
    next.hit = true;

    // Baseline hitCheck -> getCriticalHitResult -> damage RNG ordering:
    // stage 0 is 1/24; the random damage factor is inclusive [85, 100].
    next.criticalRoll = static_cast<uint8_t>(battleRng.randSeedInt(24));
    next.critical = next.criticalRoll == 0;
    next.randomDamagePercent = static_cast<uint8_t>(battleRng.randSeedIntRange(85, 100));

    double stabMultiplier = 1.0;
    if (!moveIsTypeless && !sameText(move->type, "STELLAR")) {
        const char* attackerType1 = attackerForm ? attackerForm->type1 : attackerSpecies->type1;
        const char* attackerType2 = attackerForm ? attackerForm->type2 : attackerSpecies->type2;
        if (sameText(move->type, attackerType1) ||
            (attackerType2 && *attackerType2 && !sameText(attackerType2, "NONE") &&
                sameText(move->type, attackerType2))) {
            stabMultiplier = 1.5;
        }
    }

    const double criticalMultiplier = next.critical ? 1.5 : 1.0;
    const double damage = baseDamage * criticalMultiplier
        * (static_cast<double>(next.randomDamagePercent) / 100.0)
        * stabMultiplier * next.typeEffectiveness;
    if (damage > 4294967295.0) return PokemonMoveDamageResult::InvalidStats;
    const uint32_t rounded = static_cast<uint32_t>(damage);
    next.damage = rounded ? rounded : 1;
    output = next;
    return PokemonMoveDamageResult::Ok;
}

PokemonMoveActionStatus useStandardPokemonMove(
    PokemonBattleState& attacker,
    PokemonBattleState& defender,
    uint8_t moveSlot,
    bool moveIsTypeless,
    PokerogueRngAdapter& battleRng,
    PokemonMoveActionResult& output) {
    if (moveSlot >= attacker.moveCount || moveSlot >= 4 || attacker.moves[moveSlot].moveId == 0) {
        return PokemonMoveActionStatus::InvalidMoveSlot;
    }
    if (attacker.moves[moveSlot].pp == 0) return PokemonMoveActionStatus::NoPp;
    if (defender.hp == 0) return PokemonMoveActionStatus::TargetAlreadyFainted;

    PokemonMoveActionResult next{};
    next.damageResolutionStatus = resolveStandardPokemonMoveDamage(
        attacker, defender, attacker.moves[moveSlot].moveId, moveIsTypeless, battleRng, next.damageRoll);
    if (next.damageResolutionStatus != PokemonMoveDamageResult::Ok) {
        return PokemonMoveActionStatus::DamageResolutionFailed;
    }

    // A successful move attempt consumes one PP whether it hits or misses.
    --attacker.moves[moveSlot].pp;
    if (next.damageRoll.hit && next.damageRoll.damage > 0) {
        const uint16_t applied = static_cast<uint16_t>(next.damageRoll.damage < defender.hp
            ? next.damageRoll.damage : defender.hp);
        defender.hp = static_cast<uint16_t>(defender.hp - applied);
        next.damageApplied = applied;
        next.targetFainted = defender.hp == 0;
    }
    output = next;
    return PokemonMoveActionStatus::Ok;
}

PokemonAbilitySelectionResult selectPokemonAbilityIndex(
    uint16_t speciesDex,
    uint16_t hiddenAbilityRate,
    PokerogueRngAdapter& rng,
    uint8_t& outputAbilityIndex) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonAbilitySelectionResult::MissingSpecies;
    if (hiddenAbilityRate == 0) return PokemonAbilitySelectionResult::InvalidHiddenRate;

    const uint8_t regularAbilityIndex = species->ability2 == species->ability1
        ? 0 : static_cast<uint8_t>(rng.randSeedInt(2));
    const bool useHiddenAbility = species->abilityHidden != 0 && rng.randSeedInt(hiddenAbilityRate) == 0;
    outputAbilityIndex = useHiddenAbility ? 2 : regularAbilityIndex;
    return PokemonAbilitySelectionResult::Ok;
}

PokemonGenderSelectionResult selectPokemonGender(
    uint16_t speciesDex,
    PokerogueRngAdapter& rng,
    PokemonGender& outputGender) {
    const auto* species = PokerogueContent::findSpeciesByDex(speciesDex);
    if (!species) return PokemonGenderSelectionResult::MissingSpecies;

    // 65534 is the canonical encoding for upstream's null gender ratio;
    // 65535 means the importer had no value and must not become a fake gender.
    if (species->malePercentTenths == 65534) {
        outputGender = PokemonGender::Genderless;
        return PokemonGenderSelectionResult::Ok;
    }
    if (species->malePercentTenths > 1000) return PokemonGenderSelectionResult::InvalidGenderRatio;

    // Upstream consumes randSeedFloat for every non-null ratio, including 0/100.
    const double rollPercent = rng.frac() * 100.0;
    outputGender = rollPercent <= (static_cast<double>(species->malePercentTenths) / 10.0)
        ? PokemonGender::Male : PokemonGender::Female;
    return PokemonGenderSelectionResult::Ok;
}

} // namespace Pokerogue3DS
