#pragma once
#include "gfx/renderer2d.hpp"
#include <cstdint>

namespace Pokerogue3DS {

inline bool typeIEquals(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (ca != cb) return false;
        ++a;
        ++b;
    }
    return *a == *b;
}

inline uint32_t pokemonTypeColor(const char* type) {
    if (!type || !*type) return C2D_Color32(168, 168, 120, 255);
    if (typeIEquals(type, "NORMAL")) return C2D_Color32(168, 168, 120, 255);
    if (typeIEquals(type, "FIGHTING") || typeIEquals(type, "LUCHA")) return C2D_Color32(192, 48, 40, 255);
    if (typeIEquals(type, "FLYING") || typeIEquals(type, "VOLADOR")) return C2D_Color32(168, 144, 240, 255);
    if (typeIEquals(type, "POISON") || typeIEquals(type, "VENENO")) return C2D_Color32(160, 64, 160, 255);
    if (typeIEquals(type, "GROUND") || typeIEquals(type, "TIERRA")) return C2D_Color32(224, 192, 104, 255);
    if (typeIEquals(type, "ROCK") || typeIEquals(type, "ROCA")) return C2D_Color32(184, 160, 56, 255);
    if (typeIEquals(type, "BUG") || typeIEquals(type, "BICHO")) return C2D_Color32(168, 184, 32, 255);
    if (typeIEquals(type, "GHOST") || typeIEquals(type, "FANTASMA")) return C2D_Color32(112, 88, 152, 255);
    if (typeIEquals(type, "STEEL") || typeIEquals(type, "ACERO")) return C2D_Color32(184, 184, 208, 255);
    if (typeIEquals(type, "FIRE") || typeIEquals(type, "FUEGO")) return C2D_Color32(240, 128, 48, 255);
    if (typeIEquals(type, "WATER") || typeIEquals(type, "AGUA")) return C2D_Color32(104, 144, 240, 255);
    if (typeIEquals(type, "GRASS") || typeIEquals(type, "PLANTA")) return C2D_Color32(120, 200, 80, 255);
    if (typeIEquals(type, "ELECTRIC") || typeIEquals(type, "ELECTRICO") || typeIEquals(type, "ELÉCTRICO")) return C2D_Color32(248, 208, 48, 255);
    if (typeIEquals(type, "PSYCHIC") || typeIEquals(type, "PSIQUICO") || typeIEquals(type, "PSÍQUICO")) return C2D_Color32(248, 88, 136, 255);
    if (typeIEquals(type, "ICE") || typeIEquals(type, "HIELO")) return C2D_Color32(152, 216, 216, 255);
    if (typeIEquals(type, "DRAGON") || typeIEquals(type, "DRAGÓN")) return C2D_Color32(112, 56, 248, 255);
    if (typeIEquals(type, "DARK") || typeIEquals(type, "SINIESTRO")) return C2D_Color32(112, 88, 72, 255);
    if (typeIEquals(type, "FAIRY") || typeIEquals(type, "HADA")) return C2D_Color32(238, 153, 172, 255);
    return C2D_Color32(168, 168, 120, 255);
}

inline const char* pokemonTypeUiName(const char* type) {
    if (!type || !*type) return "NORMAL";
    if (typeIEquals(type, "NORMAL")) return "NORMAL";
    if (typeIEquals(type, "FIGHTING") || typeIEquals(type, "LUCHA")) return "LUCHA";
    if (typeIEquals(type, "FLYING") || typeIEquals(type, "VOLADOR")) return "VOLADOR";
    if (typeIEquals(type, "POISON") || typeIEquals(type, "VENENO")) return "VENENO";
    if (typeIEquals(type, "GROUND") || typeIEquals(type, "TIERRA")) return "TIERRA";
    if (typeIEquals(type, "ROCK") || typeIEquals(type, "ROCA")) return "ROCA";
    if (typeIEquals(type, "BUG") || typeIEquals(type, "BICHO")) return "BICHO";
    if (typeIEquals(type, "GHOST") || typeIEquals(type, "FANTASMA")) return "FANTASMA";
    if (typeIEquals(type, "STEEL") || typeIEquals(type, "ACERO")) return "ACERO";
    if (typeIEquals(type, "FIRE") || typeIEquals(type, "FUEGO")) return "FUEGO";
    if (typeIEquals(type, "WATER") || typeIEquals(type, "AGUA")) return "AGUA";
    if (typeIEquals(type, "GRASS") || typeIEquals(type, "PLANTA")) return "PLANTA";
    if (typeIEquals(type, "ELECTRIC") || typeIEquals(type, "ELECTRICO") || typeIEquals(type, "ELÉCTRICO")) return "ELÉCTRICO";
    if (typeIEquals(type, "PSYCHIC") || typeIEquals(type, "PSIQUICO") || typeIEquals(type, "PSÍQUICO")) return "PSÍQUICO";
    if (typeIEquals(type, "ICE") || typeIEquals(type, "HIELO")) return "HIELO";
    if (typeIEquals(type, "DRAGON") || typeIEquals(type, "DRAGÓN")) return "DRAGÓN";
    if (typeIEquals(type, "DARK") || typeIEquals(type, "SINIESTRO")) return "SINIESTRO";
    if (typeIEquals(type, "FAIRY") || typeIEquals(type, "HADA")) return "HADA";
    return type;
}

inline void drawTypeBadge(Renderer2D& renderer, const char* type, float x, float y, float w, float h, float textSize = 0.22f) {
    if (!type || !*type) return;
    const uint32_t col = pokemonTypeColor(type);
    renderer.drawRect(x, y, w, h, col);
    const char* label = pokemonTypeUiName(type);
    renderer.drawText(label, x + 3.0f, y + 1.0f, textSize, C2D_Color32(255, 255, 255, 255));
}

}
