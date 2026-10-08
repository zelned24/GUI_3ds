import fs from 'node:fs';
import {SpeciesDefinition} from '../tools/js/data/CanonicalModels.js';
import {PokerogueAdapter} from '../tools/js/data/PokerogueAdapter.js';
import {PokemonSpriteResolver} from '../tools/js/data/PokemonSpriteResolver.js';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
export function registerPresentationTests(test) {
  for(const script of ['item_icon_index_tests.py','ui_audio_assets_tests.py','egg_ui_import_tests.py',
      'egg_texture_assets_tests.py','item_ui_import_tests.py','berry_ui_import_tests.py','arena_alignment_tests.py'])
    test('Pinned presentation pipeline: '+script,()=>{
      execFileSync('python',[path.join(root,'test',script)],{stdio:'pipe'});
    });

  test('Native Pokedex binding: canonical species and real profile discovery without battle writes',()=>{
    const frontend=fs.readFileSync(path.join(root,'project/include/runtime/FrontendMenuPresenter.hpp'),'utf8');
    const main=fs.readFileSync(path.join(root,'project/src/main.cpp'),'utf8');
    const dex=frontend.slice(frontend.indexOf('bool dexMatches('),frontend.indexOf('unsigned rowY('));
    for(const token of ['PokerogueContent::kSpecies','starterProfileReady()','starterProgress(species.dex)',
                       'starterDiscoveryTint(starterDiscovery','"???"'])
      if(!dex.includes(token)) throw new Error('Missing real Pokedex projection: '+token);
    if(/recordCaught|recordSeen|claimReward|selectMove/.test(dex)) throw new Error('Pokedex must not write game progress');
    if(!dex.includes('species.generation!=m_dexGeneration') || !dex.includes('seen && !caught') ||
       !dex.includes('m_dexCapture==1 ? caught') || !dex.includes('if(!game || !game->starterProfileReady()) return false;'))
      throw new Error('Discovery filters must use actual profile state and reject unavailable profiles');
    const upper=frontend.slice(frontend.indexOf('void drawPokedexTop('),frontend.indexOf('void draw(Renderer2D&'));
    if(!upper.includes('if(!known) return;') || !upper.includes('species->baseTotal'))
      throw new Error('Upper details must reveal canonical stats only for known species');
    if(!main.includes('frontend.drawPokedexTop(renderer,game)')) throw new Error('Upper Pokedex details are not connected');
    if(!main.includes('frontend.draw(renderer,loaded==Pokerogue3DS::NativeSaveResult::Ok ? &restored : nullptr,&game)'))
      throw new Error('Pokedex must consume the actual game profile');
  });
  test('Native global submenu: nine pinned locale entries, touch geometry and upper-screen overlay',()=>{
    const frontend=fs.readFileSync(path.join(root,'project/include/runtime/FrontendMenuPresenter.hpp'),'utf8');
    const main=fs.readFileSync(path.join(root,'project/src/main.cpp'),'utf8');
    const locales=fs.readFileSync(path.join(root,'project/generated/include/content/RuntimeUiText.hpp'),'utf8');
    const section=frontend.slice(frontend.indexOf('static const char* const* globalMenuKeys()'),frontend.indexOf('unsigned rowCount() const'));
    const keys=[...section.matchAll(/"(menu-ui-handler:[^"]+)"/g)].map(match=>match[1]);
    if(keys.length!==9 || new Set(keys).size!==9) throw new Error('Global submenu must preserve nine distinct entries');
    for(const key of keys) if(!locales.includes('"'+key+'"')) throw new Error('Missing pinned submenu locale: '+key);
    if(!(frontend.includes('globalMenuRowRectangle(index)') && frontend.includes('rowRectangle(i).contains(touchX,touchY)') && frontend.includes('const float y=rowY(i)')) || !frontend.includes('FrontendPage::GlobalMenu ? 20 : 29'))
      throw new Error('Drawing and touch must share submenu row geometry');
    if(!main.includes('frontend.overlaysTitle()) renderer.drawRect(0,0,400,240,0x60000000)'))
      throw new Error('Global submenu must dim the upper scene');
  });
  test('Native submenu pending destinations: explicit status without account or progress writes',()=>{
    const frontend=fs.readFileSync(path.join(root,'project/include/runtime/FrontendMenuPresenter.hpp'),'utf8');
    const info=frontend.slice(frontend.indexOf('void drawServiceInfo('),frontend.indexOf('bool dexMatches('));
    for(const option of [1,3,4,7,8])
      if(!info.includes('case '+option+':')) throw new Error('Missing submenu destination '+option);
    if(!frontend.includes('m_service=m_selected;m_page=FrontendPage::ServiceInfo') ||
       !frontend.includes('m_page==FrontendPage::ServiceInfo || m_page==FrontendPage::ManageData) m_page=FrontendPage::GlobalMenu;'))
      throw new Error('Pending destinations must open and return to the global submenu');
    for(const expected of ['if(m_service==2)', 'if(m_service==3)', 'game->eggAt(selected)', 'eggHatchMessageKey(egg->hatchWaves)'])
      if(!info.includes(expected)) throw new Error('Missing read-only submenu binding: '+expected);
    if(/saveNative|recordCaught|recordSeen|commitImported|logOut\(/.test(info))
      throw new Error('Informational screens cannot mutate profile or fake network actions');
  });
  test('Native asset preparation: preserve materialized appearances and defer menu cursor release',()=>{
    const prepare=fs.readFileSync(path.join(root,'scripts/prepare_pokerogue_sprite_catalog.mjs'),'utf8');
    const title=fs.readFileSync(path.join(root,'project/include/runtime/TitleMenuPresenter.hpp'),'utf8');
    const frontend=fs.readFileSync(path.join(root,'project/include/runtime/FrontendMenuPresenter.hpp'),'utf8');
    if(!prepare.includes("['scripts/stage_pokerogue_sprite_assets.mjs', '--all', '--appearances']"))
      throw new Error('Full asset preparation must retain verified materialized appearances');
    const index=fs.readFileSync(path.join(root,'scripts/generate_pokemon_appearance_index.mjs'),'utf8');
    if(!prepare.includes("['scripts/generate_pokemon_appearance_index.mjs']") ||
       !index.includes('await checked(asset.metadataPath,asset.metadataSha256)') ||
       !index.includes('await checked(texture.path,texture.sha256)') ||
       !index.includes('Duplicate converted appearance'))
      throw new Error('Appearance index must validate physical files and unique identities');
    if(!title.includes('renderer->retireSpriteSheet(m_cursor)') || !frontend.includes('m_title.clear(renderer)'))
      throw new Error('Menu cleanup must defer textures used by queued GPU commands');
  });
  test('Native appearance index: deterministic references and corrupt inventory rejection',()=>{execFileSync(process.execPath,[path.join(root,'test/pokemon_appearance_index_tests.mjs')],{stdio:'pipe'});});
  test('Pinned species gender metadata: preserve visual eligibility without inferring from sex',()=>{execFileSync(process.execPath,[path.join(root,'test/species_gender_metadata_tests.mjs')],{stdio:'pipe'});});
  test('Native appearance rendering binding: resolved identity uses indexed assets and missing states stay explicit',()=>{
    const presenter=fs.readFileSync(path.join(root,'project/src/runtime/PokemonAtlasPresenter.cpp'),'utf8');
    const resolve=presenter.slice(presenter.indexOf('bool PokemonAtlasPresenter::atlasKey('),presenter.indexOf('bool PokemonAtlasPresenter::selectMetadata('));
    for(const token of ['appearance.appearanceResolved','appearance.shinyVariant<=2',
      'speciesGenderDifferences(pokemon.dex)','formGenderVisual(form->id)','genderSpriteFormExcluded(spriteForm)',
      'findPokemonAppearanceAsset(out.c_str(),back,female,appearance.shinyVariant,appearance.shiny)',
      'NOT_YET_SUPPORTED_POKEMON_APPEARANCE','out.clear();return false;'])
      if(!resolve.includes(token)) throw new Error('Missing appearance binding '+token);
    if((presenter.match(/atlasKey\(pokemon, back, key\)/g)||[]).length!==2)
      throw new Error('Both sprite drawing paths must resolve facing and appearance');
  });
  test('Native starter default appearance binding: actual caught metadata and legacy isolation',()=>{
    const game=fs.readFileSync(path.join(root,'project/src/game/FirstRunRuntime.cpp'),'utf8');
    const start=game.indexOf('bool FirstRunRuntime::resolveStarterFromDex(');
    const binding=game.slice(start,game.indexOf('starterActor.nature = starterNature;',start));
    if(!binding.includes('dexMetadata && dexMetadata->caughtAppearanceAttr') ||
       !binding.includes('nativeStarterDefaultAppearance(*dexMetadata,starterActor.shiny,starterActor.shinyVariant)') ||
       !binding.includes('starterActor.appearanceResolved=true'))
      throw new Error('Starter default appearance must use owned profile bits and preserve unknown legacy state');
  });
  test('Legacy sprite resolver: no invented verified registry, dimensions, hashes or fixture production records',()=>{
    const resolver=new PokemonSpriteResolver();
    if(resolver.getIndexedSpeciesIds().length || resolver.resolvePokemonSprite(25).exists)
      throw new Error('Default resolver must not publish synthetic examples as production assets');
    for(const entry of [{speciesId:25},{speciesId:25,sourceType:'TEST_FIXTURE'}, {speciesId:25,physicalVerified:true}]) {
      let rejected=false;try {resolver.registerPokemonSprite(entry);} catch {rejected=true;}
      if(!rejected) throw new Error('Incomplete/fixture/caller-asserted verification must be rejected');
    }
  });
  test('Canonical sprite references: absence stays unknown instead of guessed paths or shiny capability',()=>{
    const species=new SpeciesDefinition({id:'test-only',speciesId:25});
    for(const key of ['atlasPath','icon','atlas','frame','hasFemale','hasShiny','hasVariants'])
      if(species.sprites[key]!==null) throw new Error('Unknown sprite field was fabricated: '+key);
    const supplied=new SpeciesDefinition({id:'test-only',sprites:{hasShiny:false,atlasPath:'explicit-imported-path'}});
    if(supplied.sprites.hasShiny!==false || supplied.sprites.atlasPath!=='explicit-imported-path')
      throw new Error('Explicit imported metadata must be preserved');
    const result=new PokerogueAdapter().resolveSprite(25,'BASE',true,false,'back');
    if(result.exists || result.atlasPath || result.iconPath || result.frameIndex!==null)
      throw new Error('Adapter must not invent appearance paths or frame indices');
  });
  test('Native shiny assets: pinned palettes preserve exact pixels, alpha and deterministic provenance',()=>{execFileSync('python',[path.join(root,'test/pokemon_variant_palette_tests.py')],{stdio:'pipe'});});
  test('Native sprite texture ownership: retired atlas sheets wait for SYNCDRAW',()=>{
    const sprites=fs.readFileSync(path.join(root,'project/src/runtime/PokemonAtlasPresenter.cpp'),'utf8');
    const renderer=fs.readFileSync(path.join(root,'project/src/gfx/renderer2d.cpp'),'utf8');
    const trainer=fs.readFileSync(path.join(root,'project/src/runtime/TrainerPresenter.cpp'),'utf8');
    const bridge=fs.readFileSync(path.join(root,'project/src/runtime/QuickJSBridge.cpp'),'utf8');
    const expect=(value,message)=>{if(!value) throw new Error(message);};
    expect(sprites.includes('slot.clear(&renderer)') && sprites.includes('renderer->retireSpriteSheet(sheets[i])'),
      'Atlas replacement must retire sheets instead of freeing queued draw textures');
    expect(trainer.includes('renderer->retireSpriteSheet(m_sheet)'),
      'Trainer replacement must retain textures used by queued draws');
    const loadTrainer=trainer.slice(trainer.indexOf('bool TrainerPresenter::loadTrainer'),trainer.indexOf('bool TrainerPresenter::loadPlayerBack'));
    expect(loadTrainer.includes('clear(renderer)'), 'Unknown trainer mapping must clear the previous sprite');
    expect(!bridge.includes('.invalidate();'), 'QuickJS replacement must pass the renderer for deferred retirement');
    const begin=renderer.slice(renderer.indexOf('void Renderer2D::beginFrame()'),renderer.indexOf('void Renderer2D::retireSpriteSheet'));
    expect(begin.indexOf('C3D_FrameBegin(C3D_FRAME_SYNCDRAW)')<begin.indexOf('C2D_SpriteSheetFree(sheet)'),
      'Retired sheets must be freed after GPU synchronization');
  });
  test('Native dialogue: bounded UTF-8 pages preserve long messages and native line widths',()=>{
    const compiler=process.platform==='win32' ? 'C:/devkitPro/msys2/usr/bin/g++.exe' : 'g++';
    const output=path.join(root,'build','text-page-layout-test'+(process.platform==='win32'?'.exe':''));
    execFileSync(compiler,['-std=c++17','-O2','-I'+path.join(root,'project/include'),path.join(root,'test/native/text_page_layout_harness.cpp'),'-o',output],{stdio:'pipe'});
    execFileSync(output,[],{stdio:'pipe'});
  });
  test('Native sprite pixels: normalize animation canvases without fractional draw scales',()=>{execFileSync('python',[path.join(root,'test/native_sprite_pixel_tests.py')],{stdio:'pipe'});});
  test('Native type atlases: preserve trim offsets, sort deterministically and reject invalid frames',()=>{execFileSync('python',[path.join(root,'test/type_atlas_tests.py')],{stdio:'pipe'});});
  test('Native pixel font: binary alpha and compact sheets preserve glyphs, metrics and references',()=>{execFileSync('python',[path.join(root,'test/pixel_font_tests.py')],{stdio:'pipe'});});
  test('Native frontend review: save failures, pause input, deterministic seed and indexed trainer assets',()=>{
    const main=fs.readFileSync(path.join(root,'project/src/main.cpp'),'utf8');
    const trainer=fs.readFileSync(path.join(root,'project/src/runtime/TrainerPresenter.cpp'),'utf8');
    const converter=fs.readFileSync(path.join(root,'scripts/prepare_native_presentation.py'),'utf8');
    if(converter.includes('CanonicalFallback')) throw new Error('Generic modifier families must not pretend to be specific item sprites');
    const intro=fs.readFileSync(path.join(root,'project/src/runtime/IntroCinematicPresenter.cpp'),'utf8');
    const expect=(value,message)=>{if(!value) throw new Error(message);};
    expect(intro.includes('C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST)') && !intro.includes('GPU_LINEAR'),'Intro must use nearest texture filtering');
    const renderer=fs.readFileSync(path.join(root,'project/src/gfx/renderer2d.cpp'),'utf8');
    const setup=fs.readFileSync(path.join(root,'project/include/runtime/SetupPresenter.hpp'),'utf8');
    const rewards=fs.readFileSync(path.join(root,'project/include/runtime/RewardMenuPresenter.hpp'),'utf8');
    expect(rewards.includes('const float iconX=cx+(cardW-32)*0.5f') && rewards.includes('iconX,cardY+16,32)'),'Reward item icons retain their original 32 pixel canvases');
    expect(!setup.includes('270.0f/image.subtex->width'),'Title logo must not use the previous fractional 1.8x scale');
    expect(setup.includes('const unsigned scale=image.subtex->width<=200 ? 2 : 1'),'Title logo uses native integer multiples');
    expect(renderer.includes('float(info->tglp->cellHeight)*pixelMultiple/30.0f'), 'Native font drawing must undo Citro2D normalization');
    expect(!renderer.includes('scale*=fit'), 'Fitted text must not shrink texture pixels fractionally');
    expect(renderer.includes('float(info->lineFeed)*raster.scale'), 'Cursor height follows actual native line spacing');
    expect(main.includes('rewardInput ? rawPressed & (KEY_X | KEY_Y | KEY_L | KEY_R)'),'Native rewards must own the pressed keys before QuickJS tick');
    expect(main.includes('rawPressed=0; // Reward input'),'Reward touch must be consumed before battle/decision touch');
    expect(main.includes('game.claimRecoveryRewardChoice(static_cast<uint8_t>(member),static_cast<uint8_t>(selection.selected))'),'PP reward must submit the selected recipient and move');
    expect(main.includes('if(!partyMenu.available(game)) partyMenu.open=false;'),'Unavailable party menu must release focus for reward/decision selectors');
    expect(!main.includes('rewardMenu.togglePartySelectionMode'),'Reward navigation has one native owner');
    const sprites=fs.readFileSync(path.join(root,'project/src/runtime/PokemonAtlasPresenter.cpp'),'utf8');
    expect(sprites.includes('anchoredSpriteScale(scale, propScale)'),'Sprite drawing must respect explicit 1x/2x scales');
    expect(sprites.includes('m_trainerFrontFemale != female'),'Trainer cache identity includes gender variant');
    const menus=fs.readFileSync(path.join(root,'project/include/runtime/FrontendMenuPresenter.hpp'),'utf8');
    const hud=fs.readFileSync(path.join(root,'project/include/runtime/BattleHudPresenter.hpp'),'utf8');
    expect(/m_lastPlayerId\s*!=\s*actor\.battleState\.pokemonId/.test(hud) && !hud.includes('m_lastPlayerDex'),'EXP display must distinguish actors of the same species');
    expect(/if\(titleVisible\)\s*\{\s*battleHud\.resetExperienceDisplay\(\)/.test(main),'A new run/restore must not inherit the previous EXP animation');
    expect(hud.includes('canonicalPresentationTypes(actor.dex,actor.formId,type1,type2)'), 'HUD badges must use the actual canonical form');
    expect(hud.includes('drawHudTypeIcon(type1,player,0,dual'), 'HUD uses original compact icon variants');
    expect(!hud.includes('drawTypeBadge(renderer'), 'Type labels cannot overlap the status row');
    expect(hud.includes('drawHudIndicator(statusKey,false'), 'Status indicator must use the original atlas');
    expect(hud.includes('drawHudIndicator("owned",true'), 'Owned indicator must use the original physical asset');
    expect(!hud.includes('tagColor'), 'No synthetic status badge in battle HUD');
    expect(hud.includes('constexpr float scale = 1.0f'), 'HUD panels must preserve native pixel scale');
    expect(hud.includes('drawHudBar(false,boss,fraction'), 'HP uses the original two-tone bar atlas');
    expect(hud.includes('drawHudBar(true,false,static_cast<float>(m_expTimeline.fraction()),expX,expY)'), 'EXP uses the original patterned texture');
    expect(main.includes('true, 258.0f, 146.0f,false,game.experienceLevelCap()'), 'Player HUD remains above the feedback panel');
    expect(hud.includes('drawHudGraphic("numbers",digit'), 'HUD numbers must use original digit atlas');
    expect(hud.includes('drawHudGraphic("overlay_exp_label"'), 'EXP label must use original localized artwork');
    expect(!hud.includes('renderer.drawText(level'), 'Level numbers must not use scaled font glyphs');
    expect(hud.includes('hudLevelDigitAtlas(player,uint16_t(visibleLevel),experienceLevelCap)'), 'Capped level digit color consumes the runtime policy');
    expect(hud.includes('sizeof(displayName),displayedNameWidth,true)'),'Gender position must use measured fitted glyph width');
    expect(!hud.includes('drawTextFitted(name,'), 'Long HUD names must keep font scale and use upstream abbreviation');
    expect(!hud.includes('approxNameWidth'),'UTF-8 byte count cannot estimate glyph width');
    const title=fs.readFileSync(path.join(root,'project/include/runtime/TitleMenuPresenter.hpp'),'utf8');
    expect(menus.includes('m_title.drawCursor(renderer,25,y,labelSize)'),'Submenu cursor uses fitted text height');
    expect(title.includes('drawCursor(renderer,25,y,labelSize)'),'Title cursor uses fitted text height');
    expect(menus.includes('m_page==FrontendPage::SettingsGroup ? 180 : 249'),'Settings reserve room for their value column');
    expect(main.includes('filterTouchInput(hidKeysDown(),preferences.touchControls)'), 'Touch disable must apply before title, menus and gameplay dispatch');
    expect(menus.includes('drawBoundedDescription(renderer,runtimeUiText("settings:confirmDisableTouch")'), 'Confirmation uses bounded wrapping with native font rasters');
    expect(main.includes('uiSettings.load(preferences,&recoveredPreferences)'), 'Boot restores persisted device preferences');
    expect(main.includes('uiSettings.save(next,preferences.touchControls,preferences.hpBarSpeed,preferences.expGainsSpeed)'), 'Applied window style must reach the SD journal');
    expect(main.includes('renderer.setWindowStyle(next)'), 'Window setting must reach the renderer through a command');
    expect(menus.includes('return FrontendCommand::NextWindowStyle'), 'Window option must emit a runtime command');
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
    expect(intro.includes('screenW,screenH,opacity);') && intro.includes('float opacity=1.0f;') && intro.includes('if (elapsedMs>=kIntroTotalDurationMs)') && !intro.includes('frameB'),'Sampled intro frames remain opaque during playback; only the final transition fades');
  });

  test('Native touch layout: moves, targets, party sizes 0..6, all 76800 pixels and overflow',()=>{
    const compiler=process.platform==='win32' ? 'C:/devkitPro/msys2/usr/bin/g++.exe' : 'g++';
    const output=path.join(root,'build','dual-screen-layout-test'+(process.platform==='win32'?'.exe':''));
    fs.mkdirSync(path.dirname(output),{recursive:true});
    execFileSync(compiler,['-std=c++17','-O2','-I'+path.join(root,'project/include'),'-I'+path.join(root,'project/generated/include'),path.join(root,'test/native/dual_screen_layout_harness.cpp'),'-o',output],{stdio:'pipe'});
    execFileSync(output,[],{stdio:'pipe'});
  });
  test('Native frontend: title, canonical modes, SD load, history and settings navigation',()=>{
    const compiler=process.platform==='win32' ? 'C:/devkitPro/msys2/usr/bin/g++.exe' : 'g++';
    const output=path.join(root,'build','frontend-menu-test'+(process.platform==='win32'?'.exe':''));
    execFileSync(compiler,['-std=c++17','-O2','-idirafter',path.join(root,'test/native/host_compat'),'-I'+path.join(root,'project/include'),'-I'+path.join(root,'project/generated/include'),path.join(root,'test/native/frontend_menu_harness.cpp'),'-o',output],{stdio:'pipe'});
    execFileSync(output,[],{stdio:'pipe'});
  });

  test('Native renderer startup: required font/window, partial cleanup and retry',()=>{
    const compiler=process.platform==='win32' ? 'C:/devkitPro/msys2/usr/bin/g++.exe' : 'g++';
    const output=path.join(root,'build','renderer-init-test'+(process.platform==='win32'?'.exe':''));
    execFileSync(compiler,['-std=c++17','-O2','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-D__wasm__','-D__3DS__','-idirafter',path.join(root,'test/native/host_compat'),'-I'+path.join(root,'project/include'),'-I'+path.join(root,'project/generated/include'),path.join(root,'test/native/renderer_init_harness.cpp'),path.join(root,'project/src/gfx/renderer2d.cpp'),'-o',output],{stdio:'pipe'});
    execFileSync(output,[],{stdio:'pipe'});
  });

  test('Native trainer cache: failed loads, identity changes, recovery and texture ownership',()=>{
    const compiler=process.platform==='win32' ? 'C:/devkitPro/msys2/usr/bin/g++.exe' : 'g++';
    const output=path.join(root,'build','trainer-presenter-test'+(process.platform==='win32'?'.exe':''));
    execFileSync(compiler,['-std=c++17','-O2','-idirafter',path.join(root,'test/native/host_compat'),
      '-I'+path.join(root,'project/include'),'-I'+path.join(root,'project/generated/include'),
      path.join(root,'test/native/trainer_presenter_harness.cpp'),
      path.join(root,'project/src/runtime/TrainerPresenter.cpp'),
      path.join(root,'project/src/runtime/PokemonAtlasMetadata.cpp'),'-o',output],{stdio:'pipe'});
    execFileSync(output,[],{stdio:'pipe'});
  });

  test('Native complete intro: all source timestamps, page ownership, final hold and load failure',()=>{
    const compiler=process.platform==='win32' ? 'C:/devkitPro/msys2/usr/bin/g++.exe' : 'g++';
    const output=path.join(root,'build','intro-cinematic-test'+(process.platform==='win32'?'.exe':''));
    execFileSync(compiler,['-std=c++17','-O2','-idirafter',path.join(root,'test/native/host_compat'),
      '-I'+path.join(root,'project/include'),'-I'+path.join(root,'project/generated/include'),
      path.join(root,'test/native/intro_cinematic_harness.cpp'),
      path.join(root,'project/src/runtime/IntroCinematicPresenter.cpp'),'-o',output],{stdio:'pipe'});
    execFileSync(output,[],{stdio:'pipe'});
  });

  test('Native icon index: every starter, canonical species/forms, physical page bounds and unique references',()=>{
    const compiler=process.platform==='win32' ? 'C:/devkitPro/msys2/usr/bin/g++.exe' : 'g++';
    const output=path.join(root,'build','pokemon-icon-index-test'+(process.platform==='win32'?'.exe':''));
    execFileSync(compiler,['-std=c++17','-O2','-idirafter',path.join(root,'test/native/host_compat'),'-I'+path.join(root,'project/include'),'-I'+path.join(root,'project/generated/include'),path.join(root,'test/native/pokemon_icon_index_harness.cpp'),'-o',output],{stdio:'pipe'});
    execFileSync(output,[],{stdio:'pipe'});
  });

}
