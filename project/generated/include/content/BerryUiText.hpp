// Generated pinned berry presentation.
#pragma once
#include <cstring>
namespace Pokerogue3DS {
struct BerryUiEntry {const char* symbol;const char* name;const char* iconKey;};
inline constexpr BerryUiEntry kBerryUiEntries[]={
    {"SITRUS","Baya Zidra","sitrus_berry"},
    {"LUM","Baya Ziuela","lum_berry"},
    {"ENIGMA","Baya Enigma","enigma_berry"},
    {"LIECHI","Baya Lichi","liechi_berry"},
    {"GANLON","Baya Gonlan","ganlon_berry"},
    {"PETAYA","Baya Yapati","petaya_berry"},
    {"APICOT","Baya Aricoc","apicot_berry"},
    {"SALAC","Baya Aslac","salac_berry"},
    {"LANSAT","Baya Zonlan","lansat_berry"},
    {"STARF","Baya Arabol","starf_berry"},
    {"LEPPA","Baya Zanama","leppa_berry"},
};
inline const BerryUiEntry* berryUiEntry(const char* symbol) {if(!symbol) return nullptr;for(const auto& row:kBerryUiEntries) if(!std::strcmp(symbol,row.symbol)) return &row;return nullptr;}
}
