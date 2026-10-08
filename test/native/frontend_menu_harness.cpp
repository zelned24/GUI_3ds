#include "runtime/FrontendMenuPresenter.hpp"
#include "runtime/RewardMenuPresenter.hpp"
#include "runtime/BattleCommandMenuPresenter.hpp"
#include <cassert>
#include "content/WindowTexture.hpp"
#include "storage/NativePresentationSettings.hpp"
using namespace Pokerogue3DS;
// Navigation-only test: no GPU loads or draws are performed.
void C2D_SpriteSheetFree(C2D_SpriteSheet) {}
void Renderer2D::retireSpriteSheet(C2D_SpriteSheet) { assert(false && "Navigation test must not retire GPU textures"); }
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
    PreferenceDisk touchDisk;NativePresentationSettingsStore touches(touchDisk);
    assert(touches.save(3,false)==NativeSaveResult::Ok);
    assert(touches.load(restored)==NativeSaveResult::Ok && !restored.touchControls && restored.windowStyle==3);
    assert(touches.save(4,true)==NativeSaveResult::Ok);
    assert(touches.load(restored)==NativeSaveResult::Ok && restored.touchControls && restored.windowStyle==4);
    // Legacy version 1 has no touch preference; use the canonical default enabled.
    NativePresentationSettings legacy{1,2,false};char legacyBytes[NativePresentationSettingsStore::kBytes];
    NativePresentationSettingsStore::encode(legacy,legacyBytes);legacyBytes[8]=1;
    IntegritySha256 legacyHash;legacyHash.update(legacyBytes,20);legacyHash.finish(reinterpret_cast<uint8_t*>(legacyBytes+20));
    assert(NativePresentationSettingsStore::decode(legacyBytes,sizeof(legacyBytes),restored)==NativeSaveResult::Ok);
    assert(restored.windowStyle==2 && restored.touchControls);
    assert(restored.hpBarSpeed==0);
    NativePresentationSettingsStore::encode(legacy,legacyBytes);legacyBytes[8]=2;
    IntegritySha256 v2Hash;v2Hash.update(legacyBytes,20);v2Hash.finish(reinterpret_cast<uint8_t*>(legacyBytes+20));
    assert(NativePresentationSettingsStore::decode(legacyBytes,sizeof(legacyBytes),restored)==NativeSaveResult::Ok);
    assert(!restored.touchControls && restored.hpBarSpeed==0);
    for(unsigned speed=0;speed<4;++speed) for(bool touch:{false,true}) {
        NativePresentationSettings value{7,2,touch,speed};
        assert(NativePresentationSettingsStore::encode(value,legacyBytes)==NativeSaveResult::Ok);
        assert(NativePresentationSettingsStore::decode(legacyBytes,sizeof(legacyBytes),restored)==NativeSaveResult::Ok);
        assert(restored.hpBarSpeed==speed && restored.touchControls==touch);
    }
    PreferenceDisk roundtripDisk;NativePresentationSettingsStore roundtrip(roundtripDisk);
    for(unsigned speed=0;speed<4;++speed) {
        assert(roundtrip.save(2,false,speed)==NativeSaveResult::Ok);
        assert(roundtrip.load(restored)==NativeSaveResult::Ok);
        assert(restored.hpBarSpeed==speed && !restored.touchControls && restored.windowStyle==2);
    }
    assert(roundtrip.save(3,true)==NativeSaveResult::Ok);
    assert(roundtrip.load(restored)==NativeSaveResult::Ok && restored.hpBarSpeed==3 && restored.touchControls && restored.windowStyle==3);
    assert(roundtrip.save(2,true,4)==NativeSaveResult::InvalidRecord);
    NativePresentationSettings invalidSpeed{7,2,true,4};
    assert(NativePresentationSettingsStore::encode(invalidSpeed,legacyBytes)==NativeSaveResult::InvalidRecord);
    PreferenceDisk speedDisk;NativePresentationSettingsStore speeds(speedDisk);
    NativePresentationSettings speed0{1,2,true,0},speed1{1,2,true,1};
    NativePresentationSettingsStore::encode(speed0,speedDisk.bytes[0]);speedDisk.sizes[0]=sizeof(legacyBytes);
    NativePresentationSettingsStore::encode(speed1,speedDisk.bytes[1]);speedDisk.sizes[1]=sizeof(legacyBytes);
    assert(speeds.load(restored)==NativeSaveResult::AmbiguousJournal);
    // Unknown flags with a valid digest must not be silently discarded.
    NativePresentationSettingsStore::encode(legacy,legacyBytes);legacyBytes[10]=8;
    IntegritySha256 flagsHash;flagsHash.update(legacyBytes,20);flagsHash.finish(reinterpret_cast<uint8_t*>(legacyBytes+20));
    assert(NativePresentationSettingsStore::decode(legacyBytes,sizeof(legacyBytes),restored)==NativeSaveResult::InvalidRecord);
    // Same generation with different touch state is a conflict even if style matches.
    NativePresentationSettings yes{1,2,true},no{1,2,false};
    NativePresentationSettingsStore::encode(yes,touchDisk.bytes[0]);NativePresentationSettingsStore::encode(no,touchDisk.bytes[1]);
    assert(touches.load(restored)==NativeSaveResult::AmbiguousJournal);
    // A checksummed future version blocks fallback and overwrite.
    disk.bytes[0][8]=4;IntegritySha256 hash;hash.update(disk.bytes[0],20);hash.finish(reinterpret_cast<uint8_t*>(disk.bytes[0]+20));
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
    BattleCommandMenuPresenter combatControls;
    assert(combatControls.input(KEY_A,true)==BattleMenuCommand::None && combatControls.movesOpen());
    assert(combatControls.input(KEY_DRIGHT,true)==BattleMenuCommand::MoveColToggle);
    assert(combatControls.input(KEY_DLEFT,true)==BattleMenuCommand::MoveColToggle);
    assert(combatControls.input(KEY_DUP,true)==BattleMenuCommand::MoveRowToggle);
    assert(combatControls.input(KEY_L,true)==BattleMenuCommand::TargetPrevious);
    assert(combatControls.input(KEY_R,true)==BattleMenuCommand::TargetNext);
    for(const auto& target:kTargetButtonRects) {
        assert(combatControls.input(KEY_TOUCH,true,target.x+8,target.y+8)==BattleMenuCommand::None);
        assert(combatControls.movesOpen());
    }
    assert(combatControls.input(KEY_TOUCH,true,319,239)==BattleMenuCommand::None && combatControls.movesOpen());
    const auto back=moveBackRectangle(true);
    assert(combatControls.input(KEY_TOUCH,true,back.x,back.y)==BattleMenuCommand::None && !combatControls.movesOpen());
    combatControls.input(KEY_A,false);
    assert(combatControls.input(KEY_L,false)==BattleMenuCommand::None);
    assert(combatControls.input(KEY_R,false)==BattleMenuCommand::None);
    assert(combatControls.input(KEY_DRIGHT,false)==BattleMenuCommand::MoveColToggle);
    combatControls.input(KEY_B,false);assert(!combatControls.movesOpen());
    for(unsigned generation:{0u,1u,9u,999u}) for(unsigned start:{0u,24u,500u,2000u}) {
        const auto matches=[&](const auto& species) {return !generation || species.generation==generation;};
        const auto page=catalogSpeciesPage<24>(start,matches);
        std::array<const PokerogueContent::Species*,24> expected{};
        unsigned ordinal=0,cell=0;
        for(const auto& species:PokerogueContent::kSpecies) if(matches(species)) {
            if(ordinal>=start && cell<expected.size()) expected[cell++]=&species;
            ++ordinal;
        }
        assert(page==expected);
    }
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
    FrontendMenuPresenter global(false);
    assert(global.input(KEY_X)==FrontendCommand::None && global.page()==FrontendPage::GlobalMenu);
    assert(global.input(KEY_A)==FrontendCommand::None && global.page()==FrontendPage::Settings);
    global.input(KEY_A);assert(global.page()==FrontendPage::SettingsGroup);
    assert(global.overlaysTitle());
    global.input(KEY_B);assert(global.page()==FrontendPage::Settings);
    global.input(KEY_B);assert(global.page()==FrontendPage::GlobalMenu);
    for(unsigned row=1;row<9;++row) {
        global.input(KEY_B);global.input(KEY_X);
        for(unsigned down=0;down<row;++down) global.input(KEY_DDOWN);
        assert(global.input(KEY_A)==FrontendCommand::None);
        assert(global.page()==(row==5 ? FrontendPage::Pokedex : row==6 ? FrontendPage::ManageData : FrontendPage::ServiceInfo));
        if(row==5) {
            global.input(KEY_R);global.input(KEY_L);global.input(KEY_DRIGHT);global.input(KEY_DDOWN);
            global.input(KEY_X);global.input(KEY_Y);
            assert(global.page()==FrontendPage::Pokedex);
            for(unsigned filter=0;filter<3;++filter) global.input(KEY_Y);
            assert(global.page()==FrontendPage::Pokedex);
            global.input(KEY_B);assert(global.page()==FrontendPage::GlobalMenu);
        } else if(row==6) {
            assert(global.input(KEY_A)==FrontendCommand::ExportProgress);
            assert(global.page()==FrontendPage::ManageData);
            global.input(KEY_DDOWN);
            assert(global.input(KEY_A)==FrontendCommand::None && global.isConfirmingImport());
            global.input(KEY_B);assert(!global.isConfirmingImport());
            global.input(KEY_B);assert(global.page()==FrontendPage::GlobalMenu);
        } else {
            // Informational destinations return to their parent without a game command.
            assert(global.input(KEY_A)==FrontendCommand::None && global.page()==FrontendPage::ServiceInfo);
            global.input(KEY_B);assert(global.page()==FrontendPage::GlobalMenu);
        }
    }
    global.input(KEY_B);assert(global.page()==FrontendPage::Title);
    global.input(KEY_TOUCH,50,215);assert(global.page()==FrontendPage::GlobalMenu);
    global.input(KEY_TOUCH,50,18);assert(global.page()==FrontendPage::Settings);
    global.input(KEY_B);assert(global.page()==FrontendPage::GlobalMenu);
    global.input(KEY_TOUCH,50,196);assert(global.page()==FrontendPage::GlobalMenu);
    assert(global.input(KEY_A)==FrontendCommand::None && global.page()==FrontendPage::ServiceInfo);
    global.input(KEY_B);assert(global.page()==FrontendPage::GlobalMenu);
    global.input(KEY_B);assert(global.page()==FrontendPage::Title);
    // Every submenu row can be selected by touch and reopened after returning.
    for(unsigned row=0;row<9;++row) {
        FrontendMenuPresenter touched(false);
        touched.input(KEY_X);
        touched.input(KEY_TOUCH,50,17+row*20+10);
        if(row!=0) assert(touched.page()==FrontendPage::GlobalMenu);
        if(row!=0) touched.input(KEY_TOUCH,50,17+row*20+10);
        const auto destination=row==0 ? FrontendPage::Settings : row==5 ? FrontendPage::Pokedex : row==6 ? FrontendPage::ManageData : FrontendPage::ServiceInfo;
        assert(touched.page()==destination);
        assert(touched.overlaysTitle()==(destination!=FrontendPage::Pokedex));
        touched.input(KEY_B);assert(touched.page()==FrontendPage::GlobalMenu);
        touched.input(KEY_A);assert(touched.page()==destination);
        assert(touched.overlaysTitle()==(destination!=FrontendPage::Pokedex));
        touched.input(KEY_B);
        touched.input(KEY_TOUCH,320,17+row*20);
        assert(touched.page()==FrontendPage::GlobalMenu);
        touched.input(KEY_TOUCH,50,205);
        assert(touched.page()==FrontendPage::Title);
        assert(!touched.overlaysTitle());
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
    windowMenu.input(KEY_DDOWN);
    assert(windowMenu.input(KEY_A)==FrontendCommand::NextHpBarSpeed);
    assert(windowMenu.input(KEY_DLEFT)==FrontendCommand::PreviousHpBarSpeed);
    assert(windowMenu.input(KEY_DRIGHT)==FrontendCommand::NextHpBarSpeed);
    windowMenu.setHpBarSpeed(3);
    FrontendMenuPresenter touchMenu(false);
    touchMenu.input(KEY_TOUCH,25,48+3*29);touchMenu.input(KEY_A);
    for(unsigned i=0;i<3;++i) touchMenu.input(KEY_DDOWN);
    touchMenu.input(KEY_A);
    assert(touchMenu.page()==FrontendPage::SettingsGroup);
    assert(touchMenu.input(KEY_A)==FrontendCommand::None && touchMenu.confirmingTouchDisable());
    assert(touchMenu.input(KEY_B)==FrontendCommand::None && !touchMenu.confirmingTouchDisable());
    touchMenu.input(KEY_A);
    assert(touchMenu.input(KEY_TOUCH,32,130)==FrontendCommand::ToggleTouchControls);
    touchMenu.setTouchControls(false);
    // Physical navigation remains available after global touch filtering.
    assert(FrontendMenuPresenter::filterTouchInput(KEY_TOUCH | KEY_A,false)==KEY_A);
    assert(touchMenu.input(KEY_A)==FrontendCommand::ToggleTouchControls);
    for(unsigned bit=0;bit<32;++bit) {
        const uint32_t keys=uint32_t(1)<<bit;
        assert(FrontendMenuPresenter::filterTouchInput(keys,true)==keys);
        assert(FrontendMenuPresenter::filterTouchInput(keys,false)==(keys & ~KEY_TOUCH));
    }
    FrontendMenuPresenter saved(true);
    assert(saved.input(KEY_A)==FrontendCommand::Continue);
    saved.input(KEY_TOUCH,25,48+2*29);saved.input(KEY_A);
    assert(saved.page()==FrontendPage::Load && saved.input(KEY_A)==FrontendCommand::Load);
    saved.input(KEY_B);saved.input(KEY_TOUCH,25,48+4*29);saved.input(KEY_A);
    assert(saved.page()==FrontendPage::Settings);

    FrontendMenuPresenter dataMenu(true);
    dataMenu.input(KEY_X);
    assert(dataMenu.page()==FrontendPage::GlobalMenu);
    for(unsigned i=0;i<6;++i) dataMenu.input(KEY_DDOWN);
    assert(dataMenu.input(KEY_A)==FrontendCommand::None);
    assert(dataMenu.page()==FrontendPage::ManageData);
    assert(dataMenu.input(KEY_A)==FrontendCommand::ExportProgress);
    dataMenu.input(KEY_DDOWN);
    assert(dataMenu.input(KEY_A)==FrontendCommand::None && dataMenu.isConfirmingImport());
    assert(dataMenu.input(KEY_B)==FrontendCommand::None && !dataMenu.isConfirmingImport());
    assert(dataMenu.page()==FrontendPage::ManageData);
    dataMenu.input(KEY_A);
    assert(dataMenu.input(KEY_TOUCH,180,130)==FrontendCommand::None && !dataMenu.isConfirmingImport());
    dataMenu.input(KEY_A);
    assert(dataMenu.input(KEY_TOUCH,50,130)==FrontendCommand::ImportProgress && !dataMenu.isConfirmingImport());

    dataMenu.input(KEY_A);
    assert(dataMenu.isConfirmingImport());
    assert(dataMenu.input(KEY_A)==FrontendCommand::None && !dataMenu.isConfirmingImport()); // Default No.
    dataMenu.input(KEY_A);
    dataMenu.input(KEY_DLEFT);
    assert(dataMenu.input(KEY_A)==FrontendCommand::ImportProgress && !dataMenu.isConfirmingImport());
    dataMenu.input(KEY_B);
    assert(dataMenu.page()==FrontendPage::GlobalMenu);

    // Every global destination returns to its originating row, including nested settings.
    for(unsigned row=0;row<9;++row) {
        FrontendMenuPresenter submenu(false);
        submenu.input(KEY_X);
        for(unsigned i=0;i<row;++i) submenu.input(KEY_DDOWN);
        submenu.input(KEY_A);
        const auto destination=submenu.page();
        assert(destination!=FrontendPage::GlobalMenu);
        if(row==0) {
            submenu.input(KEY_A);
            assert(submenu.page()==FrontendPage::SettingsGroup);
            submenu.input(KEY_B);
            assert(submenu.page()==FrontendPage::Settings);
        }
        submenu.input(KEY_B);
        assert(submenu.page()==FrontendPage::GlobalMenu);
        submenu.input(KEY_A);
        assert(submenu.page()==destination); // Reopens the same destination without moving.
    }

    const auto deleteTouch=[](unsigned x,unsigned y,FrontendCommand expected,bool remainsOpen) {
        FrontendMenuPresenter menu(true);
        menu.input(KEY_DDOWN);menu.input(KEY_DDOWN);menu.input(KEY_A);
        assert(menu.page()==FrontendPage::Load);
        menu.input(KEY_X);assert(menu.isConfirmingDelete());
        assert(menu.input(KEY_TOUCH,x,y)==expected);
        assert(menu.isConfirmingDelete()==remainsOpen);
    };
    deleteTouch(32,125,FrontendCommand::DeleteSave,false);
    deleteTouch(151,154,FrontendCommand::DeleteSave,false);
    deleteTouch(160,125,FrontendCommand::None,false);
    deleteTouch(287,154,FrontendCommand::None,false);
    for(unsigned x:{0u,24u,31u,152u,159u,288u,319u,320u}) deleteTouch(x,134,FrontendCommand::None,true);
    deleteTouch(32,124,FrontendCommand::None,true);
    deleteTouch(32,155,FrontendCommand::None,true);

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
