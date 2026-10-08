#pragma once
#include "content/EggContentPolicy.hpp"
#include "content/PokerogueRuntimeContent.hpp"
#include <cstddef>
#include <cstdint>

namespace Pokerogue3DS {
// Pinned EggData fields. Timestamp is supplied metadata, never generated here
// or used as simulation entropy. Binary persistence remains a separate codec.
struct EggIncubationRecord {
    uint32_t id=0;
    uint16_t speciesDex=0;
    EggTier tier=EggTier::COMMON;
    EggSourceType sourceType=EggSourceType::GACHA_MOVE;
    int32_t hatchWaves=0;
    uint64_t timestamp=0;
    VariantTier variantTier=VariantTier::STANDARD;
    bool isShiny=false;
    uint8_t eggMoveIndex=0;
    bool overrideHiddenAbility=false;
};
enum class EggIncubationResult : uint8_t {Ok, InvalidInput, InvalidTier, InvalidSource, MissingSpecies, DuplicateId, OutputTooSmall, InvalidVariant, InvalidEggMove};
inline EggIncubationResult validateEggIncubationRecord(const EggIncubationRecord& egg) {
    if(static_cast<unsigned>(egg.tier)>=sizeof(kEggIncubationPolicies)/sizeof(kEggIncubationPolicies[0]))
        return EggIncubationResult::InvalidTier;
    if(static_cast<unsigned>(egg.sourceType)>static_cast<unsigned>(EggSourceType::EVENT))
        return EggIncubationResult::InvalidSource;
    if(static_cast<unsigned>(egg.variantTier)>static_cast<unsigned>(VariantTier::EPIC)) return EggIncubationResult::InvalidVariant;
    if(egg.eggMoveIndex>3) return EggIncubationResult::InvalidEggMove;
    // Legacy upstream eggs can have species=0 before resolution.
    if(egg.speciesDex && !PokerogueContent::findSpeciesByDex(egg.speciesDex))
        return EggIncubationResult::MissingSpecies;
    return EggIncubationResult::Ok;
}
// EggLapsePhase.start: decrement all eggs, then collect ready eggs in inventory
// order. Call only at the victory phase boundary, excluding a final classic wave.
// No hatch removal, RNG, rewards or unlocks occur here. Validate the entire batch
// before mutation; a failed call leaves records, output IDs and count unchanged.
inline EggIncubationResult lapseEggIncubation(EggIncubationRecord* eggs,size_t count,
    uint32_t* readyIds,size_t capacity,size_t& readyCount) {
    if((count && !eggs) || (capacity && !readyIds)) return EggIncubationResult::InvalidInput;
    // Reject overlapping input/output buffers before publishing IDs.
    if(count && capacity) {
        if(count>SIZE_MAX/sizeof(*eggs) || capacity>SIZE_MAX/sizeof(*readyIds))
            return EggIncubationResult::InvalidInput;
        const uintptr_t input=reinterpret_cast<uintptr_t>(eggs);
        const uintptr_t output=reinterpret_cast<uintptr_t>(readyIds);
        const size_t inputBytes=count*sizeof(*eggs),outputBytes=capacity*sizeof(*readyIds);
        if(input<=output ? output-input<inputBytes : input-output<outputBytes)
            return EggIncubationResult::InvalidInput;
    }
    size_t required=0;
    for(size_t i=0;i<count;++i) {
        const auto status=validateEggIncubationRecord(eggs[i]);
        if(status!=EggIncubationResult::Ok) return status;
        for(size_t j=0;j<i;++j) if(eggs[i].id==eggs[j].id) return EggIncubationResult::DuplicateId;
        if(eggs[i].hatchWaves<=1) ++required;
    }
    if(required>capacity) return EggIncubationResult::OutputTooSmall;
    size_t written=0;
    for(size_t i=0;i<count;++i) {
        // Preserve ready state without signed overflow if a corrupt/legacy record
        // already contains INT32_MIN. Actual hatching removes the ready records.
        if(eggs[i].hatchWaves>0) --eggs[i].hatchWaves;
        if(eggs[i].hatchWaves<1) readyIds[written++]=eggs[i].id;
    }
    readyCount=written;
    return EggIncubationResult::Ok;
}
}
