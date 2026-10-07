#pragma once
#include "gfx/renderer2d.hpp"
#include "content/ItemIcons.hpp"
#include "content/ItemIconReferences.hpp"
#include <cstdio>
#include <cstring>

namespace Pokerogue3DS {

class ItemIconPresenter {
public:
    ItemIconPresenter() = default;
    ItemIconPresenter(const ItemIconPresenter&) = delete;
    ItemIconPresenter& operator=(const ItemIconPresenter&) = delete;
    ~ItemIconPresenter() { clear(); }

    void clear() {
        if (m_sheet) C2D_SpriteSheetFree(m_sheet);
        if (m_looseSheet) C2D_SpriteSheetFree(m_looseSheet);
        m_sheet = nullptr;
        m_looseSheet = nullptr;
        m_page = 0xffff;
        m_looseKey[0] = '\0';
    }

    bool draw(Renderer2D& renderer, const char* key, float x, float y, float size = 24, float opacity = 1.0f) {
        if (!key || !*key) return false;
        const ItemIconFrame* frame = nullptr;
        for (const auto& row : kItemIconFrames) {
            if (std::strcmp(row.key, key) == 0) { frame = &row; break; }
        }
        if (frame) {
            if (!m_sheet || m_page != frame->page) {
                if (m_sheet) renderer.retireSpriteSheet(m_sheet);
                m_page = frame->page;
                m_sheet = C2D_SpriteSheetLoad(kItemIconPages[m_page]);
                if (!m_sheet && m_page == 0) {
                    m_sheet = C2D_SpriteSheetLoad("romfs:/presentation/ui/items-0.t3x");
                }
            }
            if (m_sheet) {
                C2D_Image img = C2D_SpriteSheetGetImage(m_sheet, 0);
                if (img.tex) C3D_TexSetFilter(img.tex, GPU_NEAREST, GPU_NEAREST);
                Renderer2D::AtlasFrame rect{
                    frame->x, frame->y, frame->width, frame->height,
                    frame->sourceWidth, frame->sourceHeight, frame->trimX, frame->trimY
                };
                renderer.drawAtlasFrame(img, rect, x, y, size, size, opacity);
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
    char m_looseKey[64]{};
};

} // namespace Pokerogue3DS
