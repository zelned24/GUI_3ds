#pragma once
#include "game/EggIncubation.hpp"
#include <cmath>
namespace Pokerogue3DS {
// Egg.rollSpecies weight only: JS numbers map to double, clamp before the
// upstream arithmetic, then floor. Caller supplies canonical base starter cost.
// No profile cost reduction, region multiplier, RNG or live state mutation here.
inline EggIncubationResult eggSpeciesWeight(EggTier tier,double starterCost,uint32_t& output) {
    const unsigned index=static_cast<unsigned>(tier);
    if(index>=sizeof(kEggSpeciesCostBounds)/sizeof(kEggSpeciesCostBounds[0]))
        return EggIncubationResult::InvalidTier;
    if(!std::isfinite(starterCost)) return EggIncubationResult::InvalidInput;
    const auto& bounds=kEggSpeciesCostBounds[index];
    const double cost=starterCost<bounds.minimum ? bounds.minimum :
        starterCost>bounds.maximum ? bounds.maximum : starterCost;
    const double weight=std::floor((((bounds.maximum-cost)/(bounds.maximum-bounds.minimum+1))
        *kEggSpeciesCostBoost+1)*kEggSpeciesWeightScale);
    if(!std::isfinite(weight) || weight<1 || weight>UINT32_MAX) return EggIncubationResult::InvalidInput;
    output=static_cast<uint32_t>(weight);return EggIncubationResult::Ok;
}

// General pool only; special Manaphy and legendary-focus branches occur before
// this upstream filter. Numeric SpeciesId object keys iterate in ascending order.
inline bool excludedFromGeneralEggPool(uint16_t dex) {
    for(const auto excluded:kExcludedEggSpecies) if(dex==excluded) return true;
    return false;
}
inline EggIncubationResult generalEggSpeciesPool(EggTier tier,uint16_t* output,size_t capacity,size_t& count) {
    if(static_cast<unsigned>(tier)>=sizeof(kEggIncubationPolicies)/sizeof(kEggIncubationPolicies[0]))
        return EggIncubationResult::InvalidTier;
    size_t required=0;
    for(const auto& row:kSpeciesEggTiers)
        if(row.declared && row.tier==tier && !excludedFromGeneralEggPool(row.dex)) ++required;
    if(required>capacity) return EggIncubationResult::OutputTooSmall;
    if(required && !output) return EggIncubationResult::InvalidInput;
    if(capacity>SIZE_MAX/sizeof(*output)) return EggIncubationResult::InvalidInput;
    const uintptr_t first=reinterpret_cast<uintptr_t>(output),counter=reinterpret_cast<uintptr_t>(&count);
    if(required && (first<=counter ? counter-first<required*sizeof(*output) : first-counter<sizeof(count)))
        return EggIncubationResult::InvalidInput;
    size_t written=0;
    for(const auto& row:kSpeciesEggTiers)
        if(row.declared && row.tier==tier && !excludedFromGeneralEggPool(row.dex)) output[written++]=row.dex;
    count=written;return EggIncubationResult::Ok;
}

enum class EggSpeciesDrawResult : uint8_t {Ok,InvalidInput,InvalidTier,MissingSpecies,InvalidCost,DuplicateSpecies,WeightOverflow,InvalidRoll,InvalidMembership};
// Caller owns the filtered upstream-order pool (unlock pity/variants/exclusions).
// Validate every record before selection; supplied draw follows randSeedInt(total).
// No RNG is consumed here and failed calls preserve both outputs.
inline EggSpeciesDrawResult eggSpeciesPoolWeight(EggTier tier,const uint16_t* pool,size_t count,uint32_t& total) {
    if(static_cast<unsigned>(tier)>=sizeof(kEggSpeciesCostBounds)/sizeof(kEggSpeciesCostBounds[0]))
        return EggSpeciesDrawResult::InvalidTier;
    if(!pool || !count || count>sizeof(PokerogueContent::kSpecies)/sizeof(PokerogueContent::kSpecies[0]))
        return EggSpeciesDrawResult::InvalidInput;
    uint32_t sum=0;
    for(size_t i=0;i<count;++i) {
        const auto* species=PokerogueContent::findSpeciesByDex(pool[i]);
        if(!species) return EggSpeciesDrawResult::MissingSpecies;
        bool declaredTier=false;
        for(const auto& row:kSpeciesEggTiers)
            if(row.dex==pool[i]) {declaredTier=row.declared && row.tier==tier;break;}
        if(!declaredTier || excludedFromGeneralEggPool(pool[i])) return EggSpeciesDrawResult::InvalidMembership;
        if(species->starterCost<0) return EggSpeciesDrawResult::InvalidCost;
        for(size_t previous=0;previous<i;++previous)
            if(pool[previous]==pool[i]) return EggSpeciesDrawResult::DuplicateSpecies;
        uint32_t weight=0;
        if(eggSpeciesWeight(tier,species->starterCost,weight)!=EggIncubationResult::Ok)
            return EggSpeciesDrawResult::InvalidCost;
        if(sum>UINT32_MAX-weight) return EggSpeciesDrawResult::WeightOverflow;
        sum+=weight;
    }
    total=sum;return EggSpeciesDrawResult::Ok;
}
inline EggSpeciesDrawResult eggSpeciesForWeightedRoll(EggTier tier,const uint16_t* pool,size_t count,
    uint32_t roll,uint16_t& output) {
    uint32_t total=0;
    const auto result=eggSpeciesPoolWeight(tier,pool,count,total);
    if(result!=EggSpeciesDrawResult::Ok) return result;
    if(roll>=total) return EggSpeciesDrawResult::InvalidRoll;
    uint32_t cumulative=0;
    for(size_t i=0;i<count;++i) {
        uint32_t weight=0;
        eggSpeciesWeight(tier,PokerogueContent::findSpeciesByDex(pool[i])->starterCost,weight);
        cumulative+=weight;
        if(roll<cumulative) {output=pool[i];return EggSpeciesDrawResult::Ok;}
    }
    return EggSpeciesDrawResult::InvalidRoll;
}

// Egg.rollSpecies: the locked subpool is used only when the guarantee is
// active AND at least one species is both uncaught and absent from inventory.
inline bool useLockedEggSpeciesPool(uint32_t unlockPity,size_t lockedCount) {
    return unlockPity>=kEggUnlockPityThreshold && lockedCount>0;
}
// Applied after selecting a species. Separate from rarity pity; no live mutation.
inline uint32_t eggUnlockPityAfterSelection(uint32_t previous,bool caught,bool alreadyInEggs) {
    if(!caught && !alreadyInEggs) return 0;
    return previous>=kEggUnlockPityCap ? kEggUnlockPityCap : previous+1;
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

struct EggPityState {uint32_t common=0,rare=0,epic=0,legendary=0;};
struct EggTierPityPlan {EggTier tier=EggTier::COMMON;EggPityState pity{};};
enum class EggPityResult : uint8_t {Ok,InvalidTier,InvalidSource,CounterOverflow};
// Egg.checkForPityTierOverrides: increments precede overrides; only COMMON is
// promoted. Higher-tier guarantees have priority, and only the final tier resets.
// Call for pulled eggs, before ID/species generation. No live state is mutated.
inline EggPityResult planEggTierPity(EggTier rolled,EggSourceType source,
    const EggPityState& previous,EggTierPityPlan& output) {
    if(static_cast<unsigned>(rolled)>=sizeof(kEggIncubationPolicies)/sizeof(kEggIncubationPolicies[0]))
        return EggPityResult::InvalidTier;
    if(static_cast<unsigned>(source)>static_cast<unsigned>(EggSourceType::EVENT)) return EggPityResult::InvalidSource;
    const unsigned legendaryIncrement=1+(source==EggSourceType::GACHA_LEGENDARY ? kEggGachaThresholds.legendaryOffset : 0);
    if(previous.rare==UINT32_MAX || previous.epic==UINT32_MAX || previous.legendary>UINT32_MAX-legendaryIncrement)
        return EggPityResult::CounterOverflow;
    EggTierPityPlan candidate{rolled,previous};
    ++candidate.pity.rare;++candidate.pity.epic;candidate.pity.legendary+=legendaryIncrement;
    if(rolled==EggTier::COMMON) {
        if(candidate.pity.legendary>=kEggPityThresholds.legendary) candidate.tier=EggTier::LEGENDARY;
        else if(candidate.pity.epic>=kEggPityThresholds.epic) candidate.tier=EggTier::EPIC;
        else if(candidate.pity.rare>=kEggPityThresholds.rare) candidate.tier=EggTier::RARE;
    }
    switch(candidate.tier) {
        case EggTier::COMMON:candidate.pity.common=0;break;
        case EggTier::RARE:candidate.pity.rare=0;break;
        case EggTier::EPIC:candidate.pity.epic=0;break;
        case EggTier::LEGENDARY:candidate.pity.legendary=0;break;
    }
    output=candidate;return EggPityResult::Ok;
}
struct EggVoucherPlan {VoucherType voucher=VoucherType::REGULAR;uint32_t remaining=0;uint16_t pulls=0;};
enum class EggVoucherResult : uint8_t {Ok,Cancelled,InvalidOption,InventoryFull,InsufficientVouchers};
// Imported menu choice -> validated command plan. Caller publishes voucher use
// only together with the generated eggs and pity ledger in a durable transaction.
inline EggVoucherResult planEggVoucherPull(unsigned cursor,uint32_t inventoryCount,
    const uint32_t (&voucherCounts)[4],EggVoucherPlan& output,bool freePullOverride=false) {
    constexpr size_t offers=sizeof(kEggVoucherOffers)/sizeof(kEggVoucherOffers[0]);
    if(cursor==offers) return EggVoucherResult::Cancelled;
    if(cursor>offers) return EggVoucherResult::InvalidOption;
    const auto& offer=kEggVoucherOffers[cursor];
    const unsigned index=static_cast<unsigned>(offer.voucher);
    if(index>=4) return EggVoucherResult::InvalidOption;
    if(!freePullOverride) {
        if(inventoryCount>kEggGachaInventoryLimit || offer.pulls>kEggGachaInventoryLimit-inventoryCount)
            return EggVoucherResult::InventoryFull;
        if(voucherCounts[index]<offer.consumed) return EggVoucherResult::InsufficientVouchers;
    }
    output={offer.voucher,freePullOverride ? voucherCounts[index] : voucherCounts[index]-offer.consumed,offer.pulls};
    return EggVoucherResult::Ok;
}

}
