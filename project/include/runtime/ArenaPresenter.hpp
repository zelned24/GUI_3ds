#pragma once
#include "gfx/renderer2d.hpp"
#include "content/ArenaTextures.hpp"
#include "content/ArenaLayerTextures.hpp"
#include <cstdio>
#include "runtime/PokemonAtlasMetadata.hpp"
#include "runtime/TrainerPresenter.hpp"
#include "runtime/PokemonAtlasPresenter.hpp"
#include "runtime/PresentationClock.hpp"
#include <cstring>
#include <string>
namespace Pokerogue3DS {
// Owns only the visible background; no gameplay or guessed asset paths.
class ArenaPresenter {
public:
    ~ArenaPresenter() { clear(); }
    ArenaPresenter() = default;
    ArenaPresenter(const ArenaPresenter&) = delete;
    ArenaPresenter& operator=(const ArenaPresenter&) = delete;
    void clear(Renderer2D* renderer=nullptr) {
        if (m_sheet) {
            if(renderer) renderer->retireSpriteSheet(m_sheet);
            else C2D_SpriteSheetFree(m_sheet);
        }
        m_sheet = nullptr;
        m_definition = nullptr;
        for (unsigned i=0;i<2;++i) {
            if (m_layers[i]) {
                if(renderer) renderer->retireSpriteSheet(m_layers[i]);
                else C2D_SpriteSheetFree(m_layers[i]);
            }
            m_layers[i]=nullptr; m_layerDefinitions[i]=nullptr; m_layerLookupAttempted[i]=false; m_layerMetadata[i].clear();
        }
        m_trainer.clear(renderer);
        m_trainerCurrentTypeId = 0;
        m_trainerAttempted=false;m_trainerCurrentFemale=false;m_trainerCurrentName.clear();
    }
    bool draw(Renderer2D& renderer, const char* biomeKey, uint64_t animationTimeMs=0, bool drawBases=true) {
        const ArenaTextureDefinition* definition = m_definition;
        if (!biomeKey || !definition || std::strcmp(definition->key,biomeKey)) {
            definition=nullptr;
            if (biomeKey) for (const auto& row : kArenaTextures)
                if (std::strcmp(row.key, biomeKey) == 0) { definition = &row; break; }
        }
        if (!definition) { clear(&renderer); return false; }
        if (definition != m_definition || m_drawBases!=drawBases) {
            clear(&renderer);
            m_definition = definition;
            m_drawBases=drawBases;
            m_sheet = C2D_SpriteSheetLoad(drawBases ? definition->path : definition->titlePath);
            if (m_sheet) {
                const auto img = C2D_SpriteSheetGetImage(m_sheet, 0);
                if (!PokemonAtlasPresenter::validAtlasImage(img,definition->width,drawBases ? definition->height : 240)) {
                    renderer.retireSpriteSheet(m_sheet);m_sheet=nullptr;
                } else C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
            }
        }
        if (!m_sheet) return false;
        // Both background rasters are prepared offline, sampled at 1:1.
        renderer.drawImageDirect(C2D_SpriteSheetGetImage(m_sheet, 0),
            0.0f, 0.0f, definition->width, drawBases ? definition->height : 240.0f);
        if (!drawBases) return true;
        for (unsigned i=0;i<2;++i) {
            const ArenaLayerTextureDefinition* layer=m_layerDefinitions[i];
            if (!m_layerLookupAttempted[i]) {
                m_layerLookupAttempted[i]=true;
                char key[96];
                const int length=std::snprintf(key,sizeof(key),"%s_%c",biomeKey,i ? 'b' : 'a');
                if (length<=0 || unsigned(length)>=sizeof(key)) continue;
                for (const auto& row:kArenaLayerTextures)
                    if (std::strcmp(row.key,key)==0) { layer=&row; break; }
            }
            if (!layer) continue;
            if (layer!=m_layerDefinitions[i]) {
                if (m_layers[i]) renderer.retireSpriteSheet(m_layers[i]);
                m_layerDefinitions[i]=layer;
                m_layers[i]=C2D_SpriteSheetLoad(layer->path);
                if (m_layers[i]) {
                    const auto img = C2D_SpriteSheetGetImage(m_layers[i], 0);
                    if (!PokemonAtlasPresenter::validAtlasImage(img,layer->width,layer->height)) {
                        renderer.retireSpriteSheet(m_layers[i]);m_layers[i]=nullptr;
                    } else C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
                }
                m_layerMetadata[i].clear();
                if (layer->metadataPath) m_layerMetadata[i].load(layer->metadataPath);
            }
            if (!m_layers[i]) continue;
            const auto image=C2D_SpriteSheetGetImage(m_layers[i],0);
            if (layer->metadataPath) {
                const auto count=m_layerMetadata[i].frameCount();
                if (!count) continue; // Invalid metadata is never a static fallback.
                const auto* frame=m_layerMetadata[i].frame(presentationAnimationFrame(animationTimeMs,12,count));
                if (!frame) continue;
                Renderer2D::AtlasFrame rectangle{frame->x,frame->y,frame->width,frame->height,
                    frame->sourceWidth,frame->sourceHeight,frame->trimX,frame->trimY};
                renderer.drawAtlasFrame(image,rectangle,0,0,
                    frame->sourceWidth,frame->sourceHeight);
            } else renderer.drawImageDirect(image,0,0,layer->width,layer->height);
        }
        return true;
    }

    bool drawTrainerBattleIntro(Renderer2D& renderer, uint16_t trainerTypeId, bool female, const char* trainerName, uint64_t animationTimeMs = 0) {
        if (trainerTypeId == 0 && (!trainerName || !*trainerName)) return false;
        const char* name=trainerName ? trainerName : "";
        if (!m_trainerAttempted || m_trainerCurrentTypeId != trainerTypeId ||
            m_trainerCurrentFemale != female || m_trainerCurrentName != name) {
            m_trainerAttempted=true;m_trainerCurrentTypeId=trainerTypeId;
            m_trainerCurrentFemale=female;m_trainerCurrentName=name;
            if (!m_trainer.loadTrainer(trainerTypeId, female,&renderer)) {
                if (*name) m_trainer.load(name,&renderer);
            }
        }
        if (!m_trainer.isLoaded()) return false;
        // Native texels: avoid fractional sampling and top clipping during the intro.
        m_trainer.drawAnchored(renderer, 265.0f, 82.0f, 1.0f, animationTimeMs);
        if (trainerName && *trainerName) {
            renderer.drawWindow(12.0f, 8.0f, 180.0f, 26.0f);
            renderer.drawTextFitted(trainerName, 20.0f, 13.0f, 0.45f, 164.0f, 0xffffffff);
        }
        return true;
    }

    TrainerPresenter& trainerPresenter() { return m_trainer; }

private:
    const ArenaTextureDefinition* m_definition = nullptr;
    C2D_SpriteSheet m_sheet = nullptr;
    bool m_drawBases=true;
    bool m_layerLookupAttempted[2]{};
    C2D_SpriteSheet m_layers[2]{};
    const ArenaLayerTextureDefinition* m_layerDefinitions[2]{};
    PokemonAtlasMetadata m_layerMetadata[2];
    TrainerPresenter m_trainer;
    uint16_t m_trainerCurrentTypeId = 0;
    bool m_trainerAttempted=false,m_trainerCurrentFemale=false;
    std::string m_trainerCurrentName;
};
}
