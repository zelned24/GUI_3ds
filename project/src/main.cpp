#include "game/FirstRunRuntime.hpp"
#include "gfx/renderer2d.hpp"
#include "runtime/ScenePlayer.hpp"
#include <3ds.h>

int main() {
    gfxInitDefault();
    Renderer2D renderer;
    if (!renderer.init()) {
        gfxExit();
        return 1;
    }

    Pokerogue3DS::FirstRunRuntime game(0x3D5C0DEu);
    Citro2D::ScenePlayer player(game.scene());
    player.enter();
    player.setLoop(true);

    while (aptMainLoop()) {
        hidScanInput();
        const uint32_t pressed = hidKeysDown();
        bool changed = false;
        if (pressed & KEY_LEFT) changed = game.cycleStarter(-1);
        else if (pressed & KEY_RIGHT) changed = game.cycleStarter(1);
        if (changed) player.load(game.scene());

        renderer.beginFrame();
        renderer.beginTop();
        player.renderTop(renderer);
        renderer.beginBottom();
        player.renderBottom(renderer);
        renderer.endFrame();
        gspWaitForVBlank();
    }

    player.exit();
    renderer.fini();
    gfxExit();
    return 0;
}
