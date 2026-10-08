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
// Egg.rollEggTier decision only. The supplied draw must be from the caller's
// resolved gacha RNG (upstream randInt, not battle randSeedInt). No draw is made
// here; guarantees/pity and voucher transactions are separate pending policies.
inline EggIncubationResult eggTierForRoll(unsigned roll,EggSourceType source,EggTier& output) {
    if(roll>=256 || static_cast<unsigned>(source)>static_cast<unsigned>(EggSourceType::EVENT))
        return EggIncubationResult::InvalidInput;
    const unsigned offset=source==EggSourceType::GACHA_LEGENDARY ? kEggGachaThresholds.legendaryOffset : 0;
    const auto tier=roll>=kEggGachaThresholds.common+offset ? EggTier::COMMON :
        roll>=kEggGachaThresholds.rare+offset ? EggTier::RARE :
        roll>=kEggGachaThresholds.epic+offset ? EggTier::EPIC : EggTier::LEGENDARY;
    output=tier;return EggIncubationResult::Ok;
}
// SpeciesDataRegistry.getEggTier, using canonical declarations and its COMMON
// fallback already normalized by the importer. Missing species remain an error.
inline EggIncubationResult speciesEggTier(uint16_t dex,EggTier& tier) {
    size_t first=0,last=sizeof(kSpeciesEggTiers)/sizeof(kSpeciesEggTiers[0]);
    while(first<last) {
        const size_t middle=first+(last-first)/2;
        if(kSpeciesEggTiers[middle].dex<dex) first=middle+1;else last=middle;
    }
    if(first>=sizeof(kSpeciesEggTiers)/sizeof(kSpeciesEggTiers[0]) || kSpeciesEggTiers[first].dex!=dex)
        return EggIncubationResult::MissingSpecies;
    tier=kSpeciesEggTiers[first].tier;return EggIncubationResult::Ok;
}
// SpeciesDataRegistry.getSpeciesForEggTier excludes COMMON fallbacks. Catalog
// eligibility only: rollSpecies applies further weights and source-specific filters.
inline EggIncubationResult speciesForEggTier(EggTier tier,uint16_t* output,size_t capacity,size_t& count) {
    if(static_cast<unsigned>(tier)>=sizeof(kEggIncubationPolicies)/sizeof(kEggIncubationPolicies[0]))
        return EggIncubationResult::InvalidTier;
    size_t required=0;
    for(const auto& row:kSpeciesEggTiers) if(row.declared && row.tier==tier) ++required;
    if(required>capacity) return EggIncubationResult::OutputTooSmall;
    if(required && !output) return EggIncubationResult::InvalidInput;
    if(capacity>SIZE_MAX/sizeof(*output)) return EggIncubationResult::InvalidInput;
    const uintptr_t first=reinterpret_cast<uintptr_t>(output),counter=reinterpret_cast<uintptr_t>(&count);
    if(required && (first<=counter ? counter-first<required*sizeof(*output) : first-counter<sizeof(count)))
        return EggIncubationResult::InvalidInput;
    size_t written=0;
    for(const auto& row:kSpeciesEggTiers) if(row.declared && row.tier==tier) output[written++]=row.dex;
    count=written;return EggIncubationResult::Ok;
}
// Egg.getEggTierDefaultHatchWaves. Tier resolution for an explicitly selected
// species is a separate upstream registry rule; caller supplies the resolved tier.
inline EggIncubationResult defaultEggIncubationWaves(uint16_t speciesDex,EggTier tier,uint16_t& waves) {
    const unsigned index=static_cast<unsigned>(tier);
    if(index>=sizeof(kEggIncubationPolicies)/sizeof(kEggIncubationPolicies[0]))
        return EggIncubationResult::InvalidTier;
    if(speciesDex && !PokerogueContent::findSpeciesByDex(speciesDex))
        return EggIncubationResult::MissingSpecies;
    for(const auto special:kSpecialEggIncubationSpecies) if(speciesDex==special) {
        waves=kManaphyEggHatchWaves;return EggIncubationResult::Ok;
    }
    waves=kEggIncubationPolicies[index].waves;return EggIncubationResult::Ok;
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
