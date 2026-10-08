// Generated pinned egg presentation.
#pragma once
#include <cstdint>
#include <cstring>
namespace Pokerogue3DS {
inline constexpr uint32_t kEggSpecialIdDivisor=204;
struct EggUiTextEntry {const char* key;const char* text;};
inline constexpr EggUiTextEntry kEggUiTexts[]={
    {"all","Todo"},
    {"defaultTier","Común"},
    {"egg","Huevo"},
    {"eggMoveUnlock","Movimiento Huevo desbloqueado:\n{{moveName}}"},
    {"eventType","Evento Misterioso"},
    {"gachaTypeLegendary","Mayor tasa de Legendario"},
    {"gachaTypeMove","Mayor tasa de Movimiento Huevo Raro"},
    {"gachaTypeShiny","Mayor tasa de variocolor"},
    {"greatTier","Raro"},
    {"hatchFromTheEgg","¡Ha salido un {{pokemonName}} del Huevo!"},
    {"hatchWavesMessageClose","A veces se mueve. Debe estar a punto de salir."},
    {"hatchWavesMessageLongTime","Parece que a este Huevo le va a costar mucho abrirse."},
    {"hatchWavesMessageNotClose","¿Qué habrá dentro? Tendrás que esperar un poco más."},
    {"hatchWavesMessageSoon","Se escuchan sonidos. ¡Pronto saldrá!"},
    {"legendaryUpGacha","¡mayor\nprob.!"},
    {"manaphyTier","Manaphy"},
    {"masterTier","Legendario"},
    {"moveUpGacha","¡Más Mov.\nHuevo Raro!"},
    {"noVouchers","¡No tienes ningún vale!"},
    {"notEnoughVouchers","¡No tienes suficientes vales!"},
    {"pull","Tirada"},
    {"pulls","Tiradas"},
    {"rareEggMoveUnlock","Movimiento Huevo Raro desbloqueado:\n{{moveName}}"},
    {"sameSpeciesEgg","¡{{species}} eclosionará de este huevo!"},
    {"selectMachine","Seleccione una máquina."},
    {"shinyUpGacha","¡Más variocolor!"},
    {"tooManyEggs","¡No tienes suficiente espacio!"},
    {"ultraTier","Épico"},
    {"vouchersExceedEggCap","¡Todos los vales sobrepasan el límite de Huevos!"},
};
inline const char* eggUiText(const char* key) {for(const auto& row:kEggUiTexts) if(key && !std::strcmp(row.key,key)) return row.text;return key;}
inline const char* eggHatchMessageKey(int32_t waves) {
    if(waves<=5) return "hatchWavesMessageSoon";
    if(waves<=15) return "hatchWavesMessageClose";
    if(waves<=50) return "hatchWavesMessageNotClose";
    return "hatchWavesMessageLongTime";
}
}
