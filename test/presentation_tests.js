import fs from 'node:fs';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
export function registerPresentationTests(test) {
  test('Native pixel font: binary alpha preserves metrics, rejects corrupt data and is deterministic',()=>{execFileSync('python',[path.join(root,'test/pixel_font_tests.py')],{stdio:'pipe'});});
  test('Native frontend review: save failures, pause input, deterministic seed and indexed trainer assets',()=>{
    const main=fs.readFileSync(path.join(root,'project/src/main.cpp'),'utf8');
    const trainer=fs.readFileSync(path.join(root,'project/src/runtime/TrainerPresenter.cpp'),'utf8');
    const converter=fs.readFileSync(path.join(root,'scripts/prepare_native_presentation.py'),'utf8');
    if(converter.includes('CanonicalFallback')) throw new Error('Generic modifier families must not pretend to be specific item sprites');
    const intro=fs.readFileSync(path.join(root,'project/src/runtime/IntroCinematicPresenter.cpp'),'utf8');
    const expect=(value,message)=>{if(!value) throw new Error(message);};
    expect(main.includes('rewardInput ? rawPressed & (KEY_X | KEY_Y | KEY_L | KEY_R)'),'Native rewards must own the pressed keys before QuickJS tick');
    expect(main.includes('rawPressed=0; // Reward input'),'Reward touch must be consumed before battle/decision touch');
    expect(main.includes('game.claimRecoveryRewardChoice(static_cast<uint8_t>(member),static_cast<uint8_t>(selection.selected))'),'PP reward must submit the selected recipient and move');
    expect(main.includes('if(!partyMenu.available(game)) partyMenu.open=false;'),'Unavailable party menu must release focus for reward/decision selectors');
    expect(!main.includes('rewardMenu.togglePartySelectionMode'),'Reward navigation has one native owner');
    const sprites=fs.readFileSync(path.join(root,'project/src/runtime/PokemonAtlasPresenter.cpp'),'utf8');
    expect(sprites.includes('anchoredSpriteScale(scale, propScale)'),'Sprite drawing must respect explicit 1x/2x scales');
    expect(sprites.includes('m_trainerFrontFemale != female'),'Trainer cache identity includes gender variant');
    const menus=fs.readFileSync(path.join(root,'project/include/runtime/FrontendMenuPresenter.hpp'),'utf8');
    const title=fs.readFileSync(path.join(root,'project/include/runtime/TitleMenuPresenter.hpp'),'utf8');
    expect(menus.includes('m_title.drawCursor(renderer,25,y,labelSize)'),'Submenu cursor uses fitted text height');
    expect(title.includes('drawCursor(renderer,25,y,labelSize)'),'Title cursor uses fitted text height');
    expect(menus.includes('m_page==FrontendPage::SettingsGroup ? 220 : 249'),'Settings reserve room for their value column');
    expect(main.includes('FrontendCommand::Continue || command==Pokerogue3DS::FrontendCommand::Load'),'Continue and Load must read the committed save, not current unsaved state');
    expect(main.includes('frontend.feedback(Pokerogue3DS::nativeSaveResultName(result))'),'SD failure must retain its specific error');
    expect(!main.includes('frontend.feedback("No hay partida guardada.")'),'Corruption/content mismatch cannot be reported as absent save');
    expect(!main.includes('osGetTime()'),'New runs must use an explicit seed');
    expect(!main.includes('1.500'),'No fabricated money display');
    expect(main.includes('rawPressed &= ~KEY_START'),'Opening pause must consume START');
    expect(main.includes('rawPressed = 0; // Pause owns this input'),'Resume must not issue a battle action');
    expect(main.includes('if (titleVisible) continue;'),'Returning to title must stop gameplay commands');
    const save=main.slice(main.indexOf('const auto saveAndReturnToTitle'),main.indexOf('bool introActive'));
    expect(save.includes('result != Pokerogue3DS::NativeSaveResult::Ok'),'Save failure must be checked');
    expect(save.indexOf('saves.load(')<save.indexOf('frontend.setHasSave(true)'),'Save metadata must be read back before success');
    expect(!trainer.includes('romfs:/presentation/trainers/%s'),'Trainer paths must come from the generated index');
    expect(intro.includes('screenW, screenH, 1.0f);'),'Crossfade base must stay opaque');
  });

  test('Native touch layout: moves, targets, party sizes 0..6, all 76800 pixels and overflow',()=>{
    const compiler=process.platform==='win32' ? 'C:/devkitPro/msys2/usr/bin/g++.exe' : 'g++';
    const output=path.join(root,'build','dual-screen-layout-test'+(process.platform==='win32'?'.exe':''));
    fs.mkdirSync(path.dirname(output),{recursive:true});
    execFileSync(compiler,['-std=c++17','-O2','-I'+path.join(root,'project/include'),path.join(root,'test/native/dual_screen_layout_harness.cpp'),'-o',output],{stdio:'pipe'});
    execFileSync(output,[],{stdio:'pipe'});
  });
  test('Native frontend: title, canonical modes, SD load, history and settings navigation',()=>{
    const compiler=process.platform==='win32' ? 'C:/devkitPro/msys2/usr/bin/g++.exe' : 'g++';
    const output=path.join(root,'build','frontend-menu-test'+(process.platform==='win32'?'.exe':''));
    execFileSync(compiler,['-std=c++17','-O2','-idirafter',path.join(root,'test/native/host_compat'),'-I'+path.join(root,'project/include'),'-I'+path.join(root,'project/generated/include'),path.join(root,'test/native/frontend_menu_harness.cpp'),'-o',output],{stdio:'pipe'});
    execFileSync(output,[],{stdio:'pipe'});
  });

  test('Native icon index: every starter, canonical species/forms, physical page bounds and unique references',()=>{
    const compiler=process.platform==='win32' ? 'C:/devkitPro/msys2/usr/bin/g++.exe' : 'g++';
    const output=path.join(root,'build','pokemon-icon-index-test'+(process.platform==='win32'?'.exe':''));
    execFileSync(compiler,['-std=c++17','-O2','-idirafter',path.join(root,'test/native/host_compat'),'-I'+path.join(root,'project/include'),'-I'+path.join(root,'project/generated/include'),path.join(root,'test/native/pokemon_icon_index_harness.cpp'),'-o',output],{stdio:'pipe'});
    execFileSync(output,[],{stdio:'pipe'});
  });

}
