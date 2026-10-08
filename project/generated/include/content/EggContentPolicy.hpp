// Generated from pinned egg enums and balance rates.
#pragma once
#include <cstdint>
namespace Pokerogue3DS {
enum class EggTier : uint8_t {
    COMMON = 0,
    RARE = 1,
    EPIC = 2,
    LEGENDARY = 3,
};
enum class EggSourceType : uint8_t {
    GACHA_MOVE = 0,
    GACHA_LEGENDARY = 1,
    GACHA_SHINY = 2,
    SAME_SPECIES_EGG = 3,
    EVENT = 4,
};
enum class VoucherType : uint8_t {
    REGULAR = 0,
    PLUS = 1,
    PREMIUM = 2,
    GOLDEN = 3,
};
struct EggIncubationPolicy { EggTier tier; uint16_t waves; };
inline constexpr EggIncubationPolicy kEggIncubationPolicies[]={
    {EggTier::COMMON,10},
    {EggTier::RARE,25},
    {EggTier::EPIC,50},
    {EggTier::LEGENDARY,100},
};
inline constexpr uint16_t kManaphyEggHatchWaves=50;
}
