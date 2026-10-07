#include "runtime/FrontendMenuPresenter.hpp"
#include "runtime/RewardMenuPresenter.hpp"
#include <cassert>
#include "content/WindowTexture.hpp"
#include "storage/NativePresentationSettings.hpp"
using namespace Pokerogue3DS;
// Navigation-only test: no GPU loads or draws are performed.
void C2D_SpriteSheetFree(C2D_SpriteSheet) {}
class PreferenceDisk final:public NativeSaveStorage {
public:
    char bytes[2][NativePresentationSettingsStore::kBytes]{};size_t sizes[2]{};
    bool interrupted=false,readError=false,corruptWrite=false;
    NativeSaveResult readSlot(unsigned slot,char* out,size_t capacity,size_t& size) override {
        size=0;if(readError) return NativeSaveResult::IoError;
        if(!sizes[slot]) return NativeSaveResult::NotFound;
        if(sizes[slot]>capacity) return NativeSaveResult::TooLarge;
        size=sizes[slot];std::memcpy(out,bytes[slot],size);return NativeSaveResult::Ok;
    }
    NativeSaveResult writeSlot(unsigned slot,const char* source,size_t size) override {
        sizes[slot]=interrupted ? size/2 : size;std::memcpy(bytes[slot],source,sizes[slot]);
        if(corruptWrite) bytes[slot][20]^=1;
        return interrupted ? NativeSaveResult::IoError : NativeSaveResult::Ok;
    }
    NativeSaveResult readExport(char*,size_t,size_t&) override {return NativeSaveResult::UnsupportedStage;}
    NativeSaveResult writeExport(const char*,size_t) override {return NativeSaveResult::UnsupportedStage;}
};
static void checkPreferences() {
    PreferenceDisk disk;NativePresentationSettingsStore store(disk);NativePresentationSettings restored{123,3};
    assert(store.load(restored)==NativeSaveResult::NotFound && restored.generation==123);
    assert(store.save(0)==NativeSaveResult::InvalidRecord && !disk.sizes[0]);
    assert(store.save(2)==NativeSaveResult::Ok);
    assert(store.load(restored)==NativeSaveResult::Ok && restored.generation==1 && restored.windowStyle==2);
    char first[NativePresentationSettingsStore::kBytes],second[NativePresentationSettingsStore::kBytes];
    assert(NativePresentationSettingsStore::encode(restored,first)==NativeSaveResult::Ok);
    assert(NativePresentationSettingsStore::encode(restored,second)==NativeSaveResult::Ok);
    assert(std::memcmp(first,second,sizeof(first))==0);
    for(size_t i=0;i<sizeof(first);++i) {
        char changed[sizeof(first)];std::memcpy(changed,first,sizeof(first));changed[i]^=1;
        NativePresentationSettings untouched{987,3};
        assert(NativePresentationSettingsStore::decode(changed,sizeof(changed),untouched)!=NativeSaveResult::Ok);
        assert(untouched.generation==987 && untouched.windowStyle==3);
    }
    disk.interrupted=true;assert(store.save(3)==NativeSaveResult::IoError);
    bool recovered=false;assert(store.load(restored,&recovered)==NativeSaveResult::Ok && recovered && restored.windowStyle==2);
    disk.interrupted=false;assert(store.save(4)==NativeSaveResult::Ok);
    assert(store.load(restored)==NativeSaveResult::Ok && restored.windowStyle==4 && restored.generation==2);
    disk.corruptWrite=true;assert(store.save(5)==NativeSaveResult::ChecksumMismatch);
    disk.corruptWrite=false;assert(store.load(restored,&recovered)==NativeSaveResult::Ok && recovered && restored.windowStyle==4);
    disk.readError=true;assert(store.save(1)==NativeSaveResult::IoError);disk.readError=false;
    NativePresentationSettings max{0xffffffffu,1};
    NativePresentationSettingsStore::encode(max,disk.bytes[1]);disk.sizes[1]=sizeof(first);
    assert(store.save(1)==NativeSaveResult::SequenceExhausted);
    NativePresentationSettings conflict{0xffffffffu,2};
    NativePresentationSettingsStore::encode(conflict,disk.bytes[0]);disk.sizes[0]=sizeof(first);
    assert(store.load(restored)==NativeSaveResult::AmbiguousJournal);
    // A checksummed future version blocks fallback and overwrite.
    disk.bytes[0][8]=2;IntegritySha256 hash;hash.update(disk.bytes[0],20);hash.finish(reinterpret_cast<uint8_t*>(disk.bytes[0]+20));
    assert(store.save(1)==NativeSaveResult::UnsupportedVersion);
    disk.sizes[0]=sizeof(first)+1;
    assert(store.save(1)==NativeSaveResult::TooLarge);
    disk.sizes[0]=sizeof(first);disk.bytes[0][8]=1;disk.bytes[0][16]=99;
    IntegritySha256 unknown;unknown.update(disk.bytes[0],20);unknown.finish(reinterpret_cast<uint8_t*>(disk.bytes[0]+20));
    assert(store.save(1)==NativeSaveResult::InvalidRecord);
    NativePresentationSettings untouched{987,3};
    assert(NativePresentationSettingsStore::decode(nullptr,52,untouched)==NativeSaveResult::InvalidFormat);
}
int main() {
    checkPreferences();
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
    assert(windowMenu.input(KEY_DLEFT)==FrontendCommand::PreviousWindowStyle);
    assert(windowMenu.input(KEY_DRIGHT)==FrontendCommand::NextWindowStyle);
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
