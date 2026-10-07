#pragma once
#include "gfx/renderer2d.hpp"
#include "content/ArenaTextures.hpp"
#include "content/ArenaLayerTextures.hpp"
#include <cstdio>
#include "runtime/PokemonAtlasMetadata.hpp"
#include "runtime/TrainerPresenter.hpp"
#include <cstring>
namespace Pokerogue3DS {
// Owns only the visible background; no gameplay or guessed asset paths.
class ArenaPresenter {
public:
    ~ArenaPresenter() { clear(); }
    ArenaPresenter() = default;
    ArenaPresenter(const ArenaPresenter&) = delete;
    ArenaPresenter& operator=(const ArenaPresenter&) = delete;
    void clear() {
        if (m_sheet) C2D_SpriteSheetFree(m_sheet);
        m_sheet = nullptr;
        m_definition = nullptr;
        for (unsigned i=0;i<2;++i) {
            if (m_layers[i]) C2D_SpriteSheetFree(m_layers[i]);
            m_layers[i]=nullptr; m_layerDefinitions[i]=nullptr; m_layerMetadata[i].clear();
        }
        m_trainer.clear();
        m_trainerCurrentTypeId = 0;
    }
    bool draw(Renderer2D& renderer, const char* biomeKey, uint64_t animationTimeMs=0, bool drawBases=true) {
        const ArenaTextureDefinition* definition = nullptr;
        if (biomeKey) for (const auto& row : kArenaTextures)
            if (std::strcmp(row.key, biomeKey) == 0) { definition = &row; break; }
        if (!definition) { clear(); return false; }
        if (definition != m_definition) {
            clear();
            m_definition = definition;
            m_sheet = C2D_SpriteSheetLoad(definition->path);
            if (m_sheet) {
                const auto img = C2D_SpriteSheetGetImage(m_sheet, 0);
                if (img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
            }
        }
        if (!m_sheet) return false;
        // Original 320x180 field retains its aspect ratio on the 400px screen.
        const float scale = drawBases ? 400.0f / definition->width
            : 240.0f / definition->height;
        const float backgroundWidth=definition->width*scale;
        renderer.drawImageDirect(C2D_SpriteSheetGetImage(m_sheet, 0),
            (400.0f-backgroundWidth)*0.5f, 0.0f, backgroundWidth, definition->height * scale);
        if (!drawBases) return true;
        for (unsigned i=0;i<2;++i) {
            char key[96];
            const int length=std::snprintf(key,sizeof(key),"%s_%c",biomeKey,i ? 'b' : 'a');
            if (length<=0 || unsigned(length)>=sizeof(key)) continue;
            const ArenaLayerTextureDefinition* layer=nullptr;
            for (const auto& row:kArenaLayerTextures)
                if (std::strcmp(row.key,key)==0) { layer=&row; break; }
            if (!layer) continue;
            if (layer!=m_layerDefinitions[i]) {
                if (m_layers[i]) C2D_SpriteSheetFree(m_layers[i]);
                m_layerDefinitions[i]=layer;
                m_layers[i]=C2D_SpriteSheetLoad(layer->path);
                if (m_layers[i]) {
                    const auto img = C2D_SpriteSheetGetImage(m_layers[i], 0);
                    if (img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
                }
                m_layerMetadata[i].clear();
                if (layer->metadataPath) m_layerMetadata[i].load(layer->metadataPath);
            }
            if (!m_layers[i]) continue;
            const auto image=C2D_SpriteSheetGetImage(m_layers[i],0);
            if (layer->metadataPath) {
                const auto count=m_layerMetadata[i].frameCount();
                if (!count) continue; // Invalid metadata is never a static fallback.
                const auto* frame=m_layerMetadata[i].frame((animationTimeMs*12/1000)%count);
                if (!frame) continue;
                Renderer2D::AtlasFrame rectangle{frame->x,frame->y,frame->width,frame->height,
                    frame->sourceWidth,frame->sourceHeight,frame->trimX,frame->trimY};
                renderer.drawAtlasFrame(image,rectangle,0,0,
                    frame->sourceWidth*scale,frame->sourceHeight*scale);
            } else renderer.drawImageDirect(image,0,0,layer->width*scale,layer->height*scale);
        }
        return true;
    }

    bool drawTrainerBattleIntro(Renderer2D& renderer, uint16_t trainerTypeId, bool female, const char* trainerName, uint64_t animationTimeMs = 0) {
        if (trainerTypeId == 0 && (!trainerName || !*trainerName)) return false;
        if (!m_trainer.isLoaded() || m_trainerCurrentTypeId != trainerTypeId) {
            m_trainerCurrentTypeId = trainerTypeId;
            if (!m_trainer.loadTrainer(trainerTypeId, female)) {
                if (trainerName) m_trainer.load(trainerName);
            }
        }
        if (!m_trainer.isLoaded()) return false;
        m_trainer.drawAnchored(renderer, 265.0f, 82.0f, 1.5f, animationTimeMs);
        if (trainerName && *trainerName) {
            renderer.drawWindow(12.0f, 8.0f, 180.0f, 26.0f);
            renderer.drawText(trainerName, 20.0f, 13.0f, 0.45f, 0xffffffff);
        }
        return true;
    }

    TrainerPresenter& trainerPresenter() { return m_trainer; }

private:
    const ArenaTextureDefinition* m_definition = nullptr;
    C2D_SpriteSheet m_sheet = nullptr;
    C2D_SpriteSheet m_layers[2]{};
    const ArenaLayerTextureDefinition* m_layerDefinitions[2]{};
    PokemonAtlasMetadata m_layerMetadata[2];
    TrainerPresenter m_trainer;
    uint16_t m_trainerCurrentTypeId = 0;
};
}
