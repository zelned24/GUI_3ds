#include "runtime/FrontendMenuPresenter.hpp"
#include "runtime/RewardMenuPresenter.hpp"
#include <cassert>
#include "content/WindowTexture.hpp"
using namespace Pokerogue3DS;
// Navigation-only test: no GPU loads or draws are performed.
void C2D_SpriteSheetFree(C2D_SpriteSheet) {}
int main() {
    assert(findWindowTexture(0)==nullptr);
    assert(findWindowTexture(999)==nullptr);
    for(const auto& row:kWindowTextures) {
        assert(findWindowTexture(row.id)==&row);
        assert(row.path && row.symbol && std::strlen(row.symbol)>0);
        for(const auto& other:kWindowTextures)
            if(&other!=&row) assert(row.id!=other.id && std::strcmp(row.path,other.path)!=0);
    }
    RewardMenuPresenter rewards;
    assert(!rewards.partySelectionMode() && !rewards.moveSelectionMode());
    rewards.setPartySelectionMode(true);
    assert(rewards.partySelectionMode() && !rewards.moveSelectionMode());
    rewards.setMoveSelectionMode(true);
    assert(!rewards.partySelectionMode() && rewards.moveSelectionMode());
    rewards.moveSelection().move(1,4);
    assert(rewards.moveSelection().selected==1);
    rewards.setPartySelectionMode(true);
    assert(rewards.partySelectionMode() && !rewards.moveSelectionMode());
    rewards.setMoveSelectionMode(true);
    assert(rewards.moveSelection().selected==0);
    rewards.resetSelection();
    assert(!rewards.partySelectionMode() && !rewards.moveSelectionMode());
    for(unsigned i=0;i<3;++i) {
        const auto rect=rewards.rectangle(i);
        assert(rewards.hitTest(rect.x,rect.y,3)==int(i));
    }
    FrontendMenuPresenter fresh(false);
    assert(fresh.input(KEY_A)==FrontendCommand::None && fresh.page()==FrontendPage::Modes);
    assert(fresh.input(KEY_A)==FrontendCommand::NewClassic);
    fresh.input(KEY_DDOWN);assert(fresh.input(KEY_A)==FrontendCommand::None);
    fresh.input(KEY_B);assert(fresh.page()==FrontendPage::Title);
    fresh.input(KEY_DDOWN);fresh.input(KEY_A);assert(fresh.page()==FrontendPage::Load);
    assert(fresh.input(KEY_A)==FrontendCommand::Load); // Empty menu permits retry after an SD failure.
    fresh.input(KEY_B);fresh.input(KEY_DDOWN);fresh.input(KEY_A);assert(fresh.page()==FrontendPage::History);
    fresh.input(KEY_B);fresh.input(KEY_DDOWN);fresh.input(KEY_A);assert(fresh.page()==FrontendPage::Settings);
    for(unsigned group=0;group<4;++group) {
        fresh.input(KEY_A);assert(fresh.page()==FrontendPage::SettingsGroup);
        assert(fresh.input(KEY_A)==FrontendCommand::None);
        fresh.input(KEY_B);assert(fresh.page()==FrontendPage::Settings);
        fresh.input(KEY_DDOWN);
    }
    // Display -> Window returns a command, leaving renderer/storage ownership outside UI.
    FrontendMenuPresenter windowMenu(false);
    windowMenu.input(KEY_TOUCH,25,48+3*29);windowMenu.input(KEY_A);
    assert(windowMenu.page()==FrontendPage::Settings);
    windowMenu.input(KEY_DDOWN);windowMenu.input(KEY_A);
    assert(windowMenu.page()==FrontendPage::SettingsGroup);
    windowMenu.input(KEY_DDOWN);
    assert(windowMenu.input(KEY_A)==FrontendCommand::NextWindowStyle);
    FrontendMenuPresenter saved(true);
    assert(saved.input(KEY_A)==FrontendCommand::Continue);
    saved.input(KEY_TOUCH,25,48+2*29);saved.input(KEY_A);
    assert(saved.page()==FrontendPage::Load && saved.input(KEY_A)==FrontendCommand::Load);
    saved.input(KEY_B);saved.input(KEY_TOUCH,25,48+4*29);saved.input(KEY_A);
    assert(saved.page()==FrontendPage::Settings);

    FrontendMenuPresenter toDelete(true);
    toDelete.input(KEY_TOUCH,25,48+2*29);toDelete.input(KEY_A);
    assert(toDelete.page()==FrontendPage::Load);
    assert(!toDelete.isConfirmingDelete());
    // Press X to prompt delete confirmation
    assert(toDelete.input(KEY_X)==FrontendCommand::None);
    assert(toDelete.isConfirmingDelete());
    // Cancel with B
    assert(toDelete.input(KEY_B)==FrontendCommand::None);
    assert(!toDelete.isConfirmingDelete());
    // Prompt again and confirm with A
    assert(toDelete.input(KEY_X)==FrontendCommand::None);
    assert(toDelete.isConfirmingDelete());
    assert(toDelete.input(KEY_A)==FrontendCommand::DeleteSave);
    assert(!toDelete.isConfirmingDelete());
    toDelete.setHasSave(false);
    assert(toDelete.input(KEY_A)==FrontendCommand::Load);
}
