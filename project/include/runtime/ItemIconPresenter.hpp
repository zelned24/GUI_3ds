#pragma once
#include "gfx/renderer2d.hpp"
#include "content/ItemIcons.hpp"
#include "content/ItemIconReferences.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace Pokerogue3DS {

class ItemIconPresenter {
public:
    ItemIconPresenter() = default;
    ItemIconPresenter(const ItemIconPresenter&) = delete;
    ItemIconPresenter& operator=(const ItemIconPresenter&) = delete;
    ~ItemIconPresenter() { clear(); }

    void clear(Renderer2D* renderer=nullptr) {
        if (m_sheet) {
            if (renderer) renderer->retireSpriteSheet(m_sheet);
            else C2D_SpriteSheetFree(m_sheet);
        }
        if (m_looseSheet) {
            if (renderer) renderer->retireSpriteSheet(m_looseSheet);
            else C2D_SpriteSheetFree(m_looseSheet);
        }
        m_sheet = nullptr;
        m_looseSheet = nullptr;
        m_page = 0xffff;
        m_failedPage = 0xffff;
        m_looseKey[0] = '\0';
    }

    bool draw(Renderer2D& renderer, const char* key, float x, float y, float size = 24, float opacity = 1.0f) {
        if (!key || !*key || !std::isfinite(x) || !std::isfinite(y) ||
            !std::isfinite(size) || size <= 0 || !std::isfinite(opacity) || opacity <= 0) return false;
        if (opacity > 1) opacity = 1;
        const ItemIconFrame* frame = nullptr;
        for (const auto& row : kItemIconFrames) {
            if (std::strcmp(row.key, key) == 0) { frame = &row; break; }
        }
        if (frame) {
            if (frame->page >= sizeof(kItemIconPages)/sizeof(kItemIconPages[0]) ||
                !frame->width || !frame->height || !frame->sourceWidth || !frame->sourceHeight) return false;
            if(m_failedPage==frame->page) return false;
            if (!m_sheet || m_page != frame->page) {
                if (m_sheet) renderer.retireSpriteSheet(m_sheet);
                m_failedPage=0xffff;
                m_page = frame->page;
                m_sheet = C2D_SpriteSheetLoad(kItemIconPages[m_page]);
                if(!m_sheet) {m_failedPage=m_page;return false;}
            }
            if (m_sheet) {
                C2D_Image img = C2D_SpriteSheetGetImage(m_sheet, 0);
                if(!img.tex || !img.subtex || !img.subtex->width || !img.subtex->height) {
                    renderer.retireSpriteSheet(m_sheet);m_sheet=nullptr;m_failedPage=m_page;return false;
                }
                if(unsigned(frame->x)+frame->width>img.subtex->width ||
                    unsigned(frame->y)+frame->height>img.subtex->height ||
                    unsigned(frame->trimX)+frame->width>frame->sourceWidth ||
                    unsigned(frame->trimY)+frame->height>frame->sourceHeight) return false;
                C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
                Renderer2D::AtlasFrame rect{
                    frame->x, frame->y, frame->width, frame->height,
                    frame->sourceWidth, frame->sourceHeight, frame->trimX, frame->trimY
                };
                renderer.drawAtlasFrame(img, rect, std::round(x), std::round(y), size, size, opacity);
                return true;
            }
        }

        return false;
    }

    bool drawItem(Renderer2D& renderer, const char* itemId, float x, float y, float size = 24, float opacity = 1.0f) {
        if (!itemId) return false;
        const char* key = findItemIconKey(itemId);
        if (key) return draw(renderer, key, x, y, size, opacity);
        return false;
    }

private:
    C2D_SpriteSheet m_sheet = nullptr;
    C2D_SpriteSheet m_looseSheet = nullptr;
    uint16_t m_page = 0xffff;
    uint16_t m_failedPage = 0xffff;
    char m_looseKey[64]{};
};

} // namespace Pokerogue3DS
