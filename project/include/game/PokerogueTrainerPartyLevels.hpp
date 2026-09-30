#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>

#include "content/PokerogueRuntimeContent.hpp"
#include "game/PokerogueEncounterResolver.hpp"
#include "game/PokerogueRngAdapter.hpp"

namespace Pokerogue3DS {

struct TrainerPartyLevels {
  uint16_t values[6]{};
  uint8_t count = 0;
  bool supported = false;
};

struct TrainerPartyTemplateChoice {
  const PokerogueContent::TrainerPartyTemplate* value = nullptr;
  uint16_t index = 0;
  bool supported = false;
};

struct TrainerPartyMemberTemplate {
  uint8_t strengthId = 0;
  uint8_t evolutionThresholdKindId = 0;
  uint8_t segmentStart = 0;
  bool sameSpecies = false;
  bool balanced = false;
  bool supported = false;
};

inline TrainerPartyMemberTemplate trainerPartyMemberTemplate(
    const PokerogueContent::TrainerPartyTemplate& partyTemplate, uint8_t memberIndex);

struct TrainerPartySpeciesChoice {
  const PokerogueContent::Species* species = nullptr;
  const char* baseSpeciesId = nullptr;
  uint8_t attempts = 0;
  bool supported = false;
};

inline const PokerogueContent::Species* trainerPartySpeciesById(const char* id) {
  if (!id) return nullptr;
  for (const auto& species : PokerogueContent::kSpecies)
    if (std::strcmp(species.id, id) == 0) return &species;
  return nullptr;
}

inline uint16_t trainerPartyRootDex(const PokerogueContent::Species& species) {
  const auto* current = &species;
  for (uint8_t depth = 0; depth < 16 && current->prevolutionDex; ++depth) {
    current = PokerogueContent::findSpeciesByDex(current->prevolutionDex);
    if (!current) return 0;
  }
  return current->prevolutionDex ? 0 : current->dex;
}

// Trainer.checkDuplicateSpecies also reserves every root from signatureSpecies.
// This lookup consumes no random draws; malformed references fail explicitly.
inline bool trainerReservedSpeciesDuplicate(const PokerogueContent::TrainerType& trainer,
    uint16_t baseSpeciesDex, bool& duplicate) {
  if (!PokerogueContent::findSpeciesByDex(baseSpeciesDex) ||
      trainer.signatureOffset > PokerogueContent::kTrainerSignatureChoiceCount ||
      trainer.signatureCount > PokerogueContent::kTrainerSignatureChoiceCount - trainer.signatureOffset)
    return false;
  bool result = false;
  for (uint8_t slot = 0; slot < trainer.signatureCount; ++slot) {
    const auto& choice = PokerogueContent::kTrainerSignatureChoices[trainer.signatureOffset + slot];
    if (choice.trainerId != trainer.id || !choice.speciesCount ||
        choice.speciesOffset > PokerogueContent::kTrainerSignatureSpeciesCount ||
        choice.speciesCount > PokerogueContent::kTrainerSignatureSpeciesCount - choice.speciesOffset)
      return false;
    for (uint8_t i = 0; i < choice.speciesCount; ++i) {
      const auto* species = trainerPartySpeciesById(
          PokerogueContent::kTrainerSignatureSpecies[choice.speciesOffset + i].speciesId);
      if (!species) return false;
      const uint16_t root = trainerPartyRootDex(*species);
      if (!root) return false;
      result |= root == baseSpeciesDex;
    }
  }
  duplicate = result;
  return true;
}

// Source order for a simple trainer pool member: tier roll, candidate roll,
// level evolution and duplicate rerolls (up to ten). Ordinary pool members
// retain that species; sameSpecies overrides it after consuming those draws.
// Balanced type rerolls still require the resolved form/type context.
inline TrainerPartySpeciesChoice resolveSimpleTrainerPoolMember(
    const PokerogueContent::TrainerType& trainer,
    const PokerogueContent::TrainerPartyTemplate& partyTemplate,
    uint8_t memberIndex, uint16_t level, uint16_t wave,
    const PokerogueContent::Species* const* previousSpecies, uint8_t previousCount,
    PokerogueRngAdapter& rng) {
  const auto member = trainerPartyMemberTemplate(partyTemplate, memberIndex);
  if (!member.supported || member.balanced ||
      !level || !wave || !trainer.speciesPoolCount ||
      (trainer.signatureCount && memberIndex + trainer.signatureCount >= partyTemplate.totalSize) ||
      previousCount > memberIndex || memberIndex >= 6) return {};
  for (uint8_t attempt = 0; attempt <= 10; ++attempt) {
    const auto pool = PokerogueEncounterResolver::resolveTrainerPoolSpecies(trainer, rng);
    if (!pool.valid) return {};
    const auto* base = trainerPartySpeciesById(pool.speciesId);
    if (!base) return {};
    const char* firstId = PokerogueEncounterResolver::resolveTrainerSpeciesForLevel(
        base->id, level, member.evolutionThresholdKindId, wave == 20, rng);
    const auto* first = trainerPartySpeciesById(firstId);
    if (!first) return {};
    bool retry = base->prevolutionDex && std::strcmp(first->id, base->id) != 0;
    const uint16_t baseRoot = trainerPartyRootDex(*base);
    if (!baseRoot) return {};
    for (uint8_t i = 0; i < previousCount; ++i) {
      if (!previousSpecies || !previousSpecies[i]) return {};
      const uint16_t priorRoot = trainerPartyRootDex(*previousSpecies[i]);
      if (!priorRoot) return {};
      retry |= priorRoot == base->dex;
    }
    bool reservedDuplicate = false;
    if (!trainerReservedSpeciesDuplicate(trainer, base->dex, reservedDuplicate)) return {};
    retry |= reservedDuplicate;
    if (retry && attempt < 10) continue;
    const char* finalId = nullptr;
    if (member.sameSpecies && memberIndex > member.segmentStart) {
      if (!previousSpecies || member.segmentStart >= previousCount ||
          !previousSpecies[member.segmentStart]) return {};
      // Trainer.genPartyMember still consumes genNewPartyMemberSpecies first.
      // Then getTrainerSpeciesForLevel(..., false) uses the segment's first actor.
      finalId = PokerogueEncounterResolver::resolveTrainerSpeciesForLevel(
          previousSpecies[member.segmentStart]->id, level, partyTemplate.parentEvolutionThresholdKindId,
          wave == 20, rng, true, false);
    } else {
      // genNewPartyMemberSpecies already evolved this species. genPartyMember
      // calls a second evolution only for a separately selected newSpeciesPool.
      finalId = first->id;
    }
    const auto* finalSpecies = trainerPartySpeciesById(finalId);
    return {finalSpecies, base->id, attempt, finalSpecies != nullptr};
  }
  return {};
}

// Initializer-installed signature callbacks occupy the final party slots in
// reverse declaration order: signature[0] is party[size - 1].
inline const char* trainerSignatureSpeciesForMember(
    const PokerogueContent::TrainerType& trainer, uint8_t partySize,
    uint8_t memberIndex, PokerogueRngAdapter& rng) {
  if (!(trainer.flags & 32U) || !partySize || partySize > 6 || memberIndex >= partySize
      || trainer.signatureCount > partySize
      || trainer.signatureOffset > PokerogueContent::kTrainerSignatureChoiceCount
      || trainer.signatureCount > PokerogueContent::kTrainerSignatureChoiceCount - trainer.signatureOffset)
    return nullptr;
  const uint8_t distanceFromEnd = static_cast<uint8_t>(partySize - memberIndex);
  if (distanceFromEnd > trainer.signatureCount) return nullptr;
  const auto& choice = PokerogueContent::kTrainerSignatureChoices[
      trainer.signatureOffset + distanceFromEnd - 1];
  if (choice.trainerId != trainer.id || choice.slot != distanceFromEnd - 1
      || !choice.speciesCount
      || choice.speciesOffset > PokerogueContent::kTrainerSignatureSpeciesCount
      || choice.speciesCount > PokerogueContent::kTrainerSignatureSpeciesCount - choice.speciesOffset)
    return nullptr;
  const uint32_t index = static_cast<uint32_t>(rng.pickIndex(choice.speciesCount));
  return PokerogueContent::kTrainerSignatureSpecies[choice.speciesOffset + index].speciesId;
}

// getRandomPartyMemberFunc chooses from the signature pool, then resolves
// evolution before addEnemyPokemon. The caller must hold the member's
// PokerogueSeedOffsetScope through this call AND actor construction.
inline const char* resolveTrainerSignatureMemberSpecies(
    const PokerogueContent::TrainerType& trainer, uint8_t partySize,
    uint8_t memberIndex, uint16_t level, uint8_t evolutionThresholdKindId,
    uint16_t wave, PokerogueRngAdapter& rng) {
  if (!level || !wave || evolutionThresholdKindId > 1) return nullptr;
  const char* selected = trainerSignatureSpeciesForMember(
      trainer, partySize, memberIndex, rng);
  if (!selected) return nullptr;
  return PokerogueEncounterResolver::resolveTrainerSpeciesForLevel(
      selected, level, evolutionThresholdKindId, wave == 20, rng);
}

// TrainerPartyTemplate accessors plus the compound offset used by
// Trainer.genPartyMember. A simple template ignores the member index;
// a compound template falls back to its parent defaults after its last slot.
inline TrainerPartyMemberTemplate trainerPartyMemberTemplate(
    const PokerogueContent::TrainerPartyTemplate& partyTemplate, uint8_t memberIndex) {
  if (memberIndex >= 6 || !partyTemplate.totalSize || partyTemplate.totalSize > 6
      || !partyTemplate.segmentCount
      || partyTemplate.segmentOffset > PokerogueContent::kTrainerPartySegmentCount
      || partyTemplate.segmentCount > PokerogueContent::kTrainerPartySegmentCount - partyTemplate.segmentOffset
      || (!partyTemplate.isCompound && partyTemplate.segmentCount != 1)) return {};
  const auto* segments = PokerogueContent::trainerPartySegmentsFor(partyTemplate);
  uint16_t total = 0;
  for (uint16_t i = 0; i < partyTemplate.segmentCount; ++i) {
    if (!segments[i].size || segments[i].size > 6 || segments[i].strengthId > 6
        || segments[i].evolutionThresholdKindId > 1) return {};
    total += segments[i].size;
  }
  if (total != partyTemplate.totalSize) return {};
  uint8_t start = 0;
  for (uint16_t i = 0; i < partyTemplate.segmentCount; ++i) {
    const auto& segment = segments[i];
    if (!partyTemplate.isCompound || memberIndex < start + segment.size)
      return {segment.strengthId, segment.evolutionThresholdKindId, start,
              segment.sameSpecies, segment.balanced, true};
    start = static_cast<uint8_t>(start + segment.size);
  }
  // Compound constructor: AVERAGE, sameSpecies=false, balanced=false,
  // default NORMAL evolution threshold. These are upstream enum values.
  return {3, 1, start, false, false, true};
}

// Trainer.genPartyMember scopes generation with executeWithSeedOffset.
// Feed this result to PokerogueSeedOffsetScope with the run's root seed;
// creation of the actor and all its random traits must occur inside that scope.
inline bool trainerPartyMemberSeedOffset(const PokerogueContent::TrainerType& trainer,
    uint32_t wave, uint8_t memberIndex, uint32_t& offset) {
  if (!wave || memberIndex >= 6) return false;
  const uint32_t derivedType = trainer.derivedTypeId;
  if (trainer.flags & 8U) {
    offset = derivedType + ((static_cast<uint32_t>(memberIndex) + 1U) << 8);
  } else {
    const uint32_t seedIndex = (trainer.flags & 16U) ? 0U : memberIndex;
    offset = wave + (derivedType << 10) + ((seedIndex + 1U) << 8);
  }
  return true;
}

// Trainer.constructor consumes a static-template draw even when getPartyTemplate
// later delegates to a callback. Keep those two lists and decisions separate.
// A supplied constructor index skips that draw, matching upstream.
// This resolves only the template; configStatus may still report unsupported
// trainer behavior (signature slots, filters, modifiers) to actor creation.
inline TrainerPartyTemplateChoice selectTrainerPartyTemplate(
    const PokerogueContent::TrainerType& trainer, uint16_t wave, PokerogueRngAdapter& rng,
    int32_t suppliedIndex = -1) {
  if (!trainer.partyTemplateCount || !trainer.partyTemplateStatus || suppliedIndex < -1) return {};
  const bool waveScaled = std::strcmp(trainer.partyTemplateStatus, "WAVE_SCALED_TEMPLATES") == 0;
  const bool waveRanges = std::strcmp(trainer.partyTemplateStatus, "CLASSIC_WAVE_RANGE_TEMPLATES") == 0;
  const bool staticCallback = std::strcmp(trainer.partyTemplateStatus, "STATIC_CALLBACK_TEMPLATE") == 0;
  const bool staticTemplates = std::strcmp(trainer.partyTemplateStatus, "STATIC_TEMPLATES") == 0
      || std::strcmp(trainer.partyTemplateStatus, "UPSTREAM_DEFAULT_TEMPLATE") == 0;
  if (!waveScaled && !waveRanges && !staticCallback && !staticTemplates) return {};
  if (trainer.partyTemplateOffset > PokerogueContent::kTrainerPartyTemplateRefCount
      || trainer.partyTemplateCount > PokerogueContent::kTrainerPartyTemplateRefCount - trainer.partyTemplateOffset) return {};
  if (waveScaled || waveRanges || staticCallback) {
    if (!trainer.callbackTemplateCount || ((waveScaled || waveRanges) && !wave)
        || (staticCallback && trainer.callbackTemplateCount != 1)
        || trainer.callbackTemplateOffset > PokerogueContent::kTrainerPartyTemplateRefCount
        || trainer.callbackTemplateCount > PokerogueContent::kTrainerPartyTemplateRefCount - trainer.callbackTemplateOffset) return {};
  }
  const uint16_t constructorIndex = suppliedIndex < 0
      ? static_cast<uint16_t>(rng.pickIndex(trainer.partyTemplateCount))
      : static_cast<uint16_t>(suppliedIndex >= trainer.partyTemplateCount ? trainer.partyTemplateCount - 1 : suppliedIndex);
  uint32_t refIndex = trainer.partyTemplateOffset + constructorIndex;
  if (waveScaled || waveRanges || staticCallback) {
    const int32_t rawIndex = waveScaled && wave > 20 ? (static_cast<int32_t>(wave) - 20 + 29) / 30 : 0;
    uint16_t callbackIndex = static_cast<uint16_t>(rawIndex >= trainer.callbackTemplateCount
        ? trainer.callbackTemplateCount - 1 : rawIndex);
    if (waveRanges) {
      callbackIndex = 0;
      while (callbackIndex + 1 < trainer.callbackTemplateCount) {
        const auto& range = PokerogueContent::kTrainerPartyTemplateRefs[trainer.callbackTemplateOffset + callbackIndex];
        if (!range.maxWave || wave <= range.maxWave) break;
        ++callbackIndex;
      }
    }
    refIndex = trainer.callbackTemplateOffset + callbackIndex;
  }
  const auto* selected = PokerogueContent::findTrainerPartyTemplate(
      PokerogueContent::kTrainerPartyTemplateRefs[refIndex].templateKey);
  return {selected, constructorIndex, selected != nullptr};
}

// Port of Trainer.getPartyLevels for non-Daily modes. Classic's upstream
// GameMode.getWaveForDifficulty returns the supplied wave unchanged.
inline TrainerPartyLevels resolveClassicTrainerPartyLevels(
    const PokerogueContent::TrainerPartyTemplate& partyTemplate,
    uint16_t wave,
    bool isDouble) {
  TrainerPartyLevels result{};
  if (wave == 0 || !trainerPartyMemberTemplate(partyTemplate, 0).supported) return result;
  const auto* segments = PokerogueContent::trainerPartySegmentsFor(partyTemplate);

  const bool expandDouble = isDouble && partyTemplate.totalSize < 2;
  const uint16_t count = expandDouble ? 2 : partyTemplate.totalSize;
  if (count == 0 || count > 6) return result;

  // Preserve upstream's binary64 operation order. Exact rational arithmetic
  // can change Math.ceil at an integer boundary after floating-point rounding.
  const double difficultyWave = wave;
  const double baseLevel = 1.0 + difficultyWave / 2.0 + std::pow(difficultyWave / 25.0, 2.0);

  for (uint16_t index = 0; index < count; ++index) {
    const auto member = trainerPartyMemberTemplate(partyTemplate, static_cast<uint8_t>(index));
    if (!member.supported) return TrainerPartyLevels{};
    const uint8_t strength = expandDouble
        ? (partyTemplate.isCompound ? 3 : segments[0].strengthId)
        : member.strengthId;
    if (strength > 6) return TrainerPartyLevels{};

    constexpr double multipliers[] = {0.95, 1.0, 1.05, 1.12, 1.22, 1.24, 1.25};
    double multiplier = multipliers[strength];
    double levelOffset = 0.0;
    if (strength < 4) {
      const double adjusted = multiplier + 0.025 * std::floor(difficultyWave / 25.0);
      multiplier = adjusted < 1.225 ? adjusted : 1.225;
      levelOffset = -std::floor((difficultyWave / 50.0) * (4 - strength));
    }
    const double level = std::ceil(baseLevel * multiplier) + levelOffset;
    if (!std::isfinite(level) || level < 1.0 || level > 65535.0) return TrainerPartyLevels{};
    result.values[index] = static_cast<uint16_t>(level);
  }
  result.count = static_cast<uint8_t>(count);
  result.supported = true;
  return result;
}

inline TrainerPartyLevels resolveClassicTrainerPartyLevels(const char* templateKey, uint16_t wave, bool isDouble) {
  const auto* partyTemplate = PokerogueContent::findTrainerPartyTemplate(templateKey);
  return partyTemplate ? resolveClassicTrainerPartyLevels(*partyTemplate, wave, isDouble) : TrainerPartyLevels{};
}

// Pinned EnemyPokemon constructor: after moveset/shiny handling, trainer
// members replace the ID-derived IVs with six draws in permanent stat order.
// trySetShiny compares IDs without consuming the member RNG; shiny variant
// selection runs in a scoped seed and restores that RNG.
inline bool generateClassicTrainerIvs(uint16_t wave, PokerogueRngAdapter& rng,
                                      uint8_t output[6]) {
  if (!wave || !output || wave / 10 > 31) return false;
  const int32_t minimum = wave / 10;
  for (uint8_t stat = 0; stat < 6; ++stat)
    output[stat] = static_cast<uint8_t>(rng.randSeedIntRange(minimum, 31));
  return true;
}

}  // namespace Pokerogue3DS
