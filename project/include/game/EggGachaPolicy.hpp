#pragma once
#include "game/EggIncubation.hpp"
namespace Pokerogue3DS {
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
