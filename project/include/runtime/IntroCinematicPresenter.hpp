#pragma once

#include "gfx/renderer2d.hpp"
#include "content/IntroCinematicData.hpp"
#include <citro2d.h>
#include <cstdint>

namespace Pokerogue3DS {

class IntroCinematicPresenter {
public:
    IntroCinematicPresenter() = default;
    IntroCinematicPresenter(const IntroCinematicPresenter&) = delete;
    IntroCinematicPresenter& operator=(const IntroCinematicPresenter&) = delete;
    ~IntroCinematicPresenter() { clear(); }

    void start();
    void skip();
    bool active() const { return m_active && !m_finished; }
    bool isFinished() const { return m_finished; }
    void clear();

    // Advances and draws the cinematic sequence onto the current top screen.
    // Returns true while the cinematic is active, false once finished.
    bool draw(Renderer2D& renderer, uint64_t currentTimestampMs = 0);

private:
    C2D_SpriteSheet m_sheet = nullptr;
    bool m_active = true;
    bool m_finished = false;
    uint64_t m_startMs = 0;
    uint64_t m_frameCounter = 0;
};

} // namespace Pokerogue3DS
