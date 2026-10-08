import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import {execFileSync} from 'node:child_process';
import {POKEROGUE_REPOSITORIES} from '../tools/js/data/PokerogueSource.js';
const frontend=await fs.readFile(new URL('../project/include/runtime/FrontendMenuPresenter.hpp',import.meta.url),'utf8');
const main=await fs.readFile(new URL('../project/src/main.cpp',import.meta.url),'utf8');
// Source guard only: this does not execute native input, storage or rendering.
for(const source of [frontend,main]) assert(!/[\u00c3\u00c2]/.test(source),'Damaged UTF-8 presentation text');
assert(frontend.includes('Generación'));
assert(frontend.includes('drawBoundedDescription'));
assert(!frontend.includes('renderer.drawTextWrapped(description'));
assert(frontend.includes('Importar reemplazará'));
assert(frontend.includes('FrontendCommand::ImportProgress'));
assert(frontend.includes('kConfirmationYesRect.contains'));
assert(frontend.includes('renderer.drawWindow(kConfirmationYesRect.x'));
assert(frontend.includes('kConfirmationNoRect.contains'));
assert(frontend.includes('renderer.drawWindow(kConfirmationNoRect.x'));
assert(main.includes('progress.readBundleCandidate'));
assert(main.includes('progressReplay().restoreNativeRunSave'));
assert(main.includes('progress.commitImported'));
assert(main.includes('loaded=saves.load(PokerogueContent::kContentHash,restored)'));
console.log('PASS frontend source guards: UTF-8 and import/export wiring; native execution pending');

const title=await fs.readFile(new URL('../project/include/runtime/TitleMenuPresenter.hpp',import.meta.url),'utf8');
assert(title.includes('if(!m_cursorAttempted)'));
assert(title.includes('m_cursor=nullptr;m_cursorAttempted=false'));

const icons=await fs.readFile(new URL('../project/include/runtime/PokemonIconPresenter.hpp',import.meta.url),'utf8');
assert(icons.includes('Slot* slot=&m_slots[icon->page]'));
assert(icons.includes('if(slot->page!=icon->page)'));
assert(icons.includes('slot.page=0xffff'));

const iconIndex=await fs.readFile(new URL('../project/generated/include/content/PokemonIcons.hpp',import.meta.url),'utf8');
const identities=[...iconIndex.matchAll(/\{(\d+),(\d+),\d+,\d+,\d+,\d+,\d+\}/g)].map(row=>[Number(row[1]),Number(row[2])]);
assert(identities.length>0);
for(let i=1;i<identities.length;++i) assert(identities[i-1][0]<identities[i][0] ||
  (identities[i-1][0]===identities[i][0] && identities[i-1][1]<identities[i][1]));
assert(icons.includes('static_assert(pokemonIconIndexOrdered()'));
assert(icons.includes('const auto* icon=findPokemonIcon(dex,formIndex)'));

assert(frontend.includes('const auto page=catalogSpeciesPage<24>(start'));
assert(!frontend.includes('*dexAt(start+cell,game)'));

assert(frontend.includes("m_globalSelection=m_selected"));
assert(frontend.includes("m_page==FrontendPage::GlobalMenu ? m_globalSelection : 0"));

const statsLabels=await fs.readFile(new URL('../project/generated/include/content/RuntimeUiText.hpp',import.meta.url),'utf8');
for(const key of ['starters','shinyStarters','speciesSeen','speciesCaught']) {
  assert(frontend.includes('game-stats-ui-handler:'+key));
  assert(statsLabels.includes('game-stats-ui-handler:'+key));
}
assert(frontend.includes('game->profileCatalogStats(stats)'));

// Check the real pinned locale, rather than merely checking generated key presence.
const locale=POKEROGUE_REPOSITORIES['pokerogue-locales'];
const statsLocale=JSON.parse(execFileSync('git',['show',locale.revision+':es-ES/game-stats-ui-handler.json'],
  {cwd:new URL('../build/upstream/pokerogue-locales/',import.meta.url),encoding:'utf8'}));
for(const key of ['starters','shinyStarters','speciesSeen','speciesCaught'])
  assert(statsLabels.includes('{'+JSON.stringify('game-stats-ui-handler:'+key)+','+JSON.stringify(statsLocale[key])+'}'));
console.log('PASS profile-statistics locale labels match pinned upstream');

// Static ownership/cache guards only; native texture I/O tests remain deferred.
const atlasSource=await fs.readFile(new URL('../project/src/runtime/PokemonAtlasPresenter.cpp',import.meta.url),'utf8');
assert(atlasSource.includes('if(slot.failedPageMask & pageBit) return false;'));
assert(atlasSource.includes('failedPageMask = 0;'));
assert(atlasSource.includes('if (!loaded) {slot.failedPageMask |= pageBit;return false;}'));
const trainerSource=await fs.readFile(new URL('../project/src/runtime/TrainerPresenter.cpp',import.meta.url),'utf8');
assert(trainerSource.includes('if(std::strcmp(m_failedKey,key)==0) return false;'));
assert(trainerSource.includes("m_failedKey[0] = '\\0';"));
assert(trainerSource.includes('std::strlen(key)>=sizeof(m_currentKey)'));
assert(trainerSource.includes('clear(renderer);\n        std::strcpy(m_failedKey,key);'));

const setup=await fs.readFile(new URL('../project/include/runtime/SetupPresenter.hpp',import.meta.url),'utf8');
assert(setup.includes('abilityUiName(game.setupStarterAbilityId(species->dex))'));
assert(!setup.includes('abilityUiName((form ? form->ability1 : species->ability1))'));

const arena=await fs.readFile(new URL('../project/include/runtime/ArenaPresenter.hpp',import.meta.url),'utf8');
assert(arena.includes('m_definition || m_drawBases!=drawBases'));
assert(arena.includes('drawBases ? definition->path : definition->titlePath'));
assert(arena.includes('frame->sourceWidth,frame->sourceHeight'));
assert(arena.includes('layer->width,layer->height'));
assert(!arena.includes('*scale'));
assert(arena.includes('presentationAnimationFrame(animationTimeMs,12,count)'));
assert(arena.includes('m_trainerCurrentFemale != female || m_trainerCurrentName != name'));
assert(arena.includes('m_trainerAttempted=false;m_trainerCurrentFemale=false;m_trainerCurrentName.clear()'));
assert(arena.includes('m_trainer.loadTrainer(trainerTypeId, female,&renderer)'));
assert(!arena.includes('if (!m_trainer.isLoaded() || m_trainerCurrentTypeId'));

const rendererSource=await fs.readFile(new URL('../project/src/gfx/renderer2d.cpp',import.meta.url),'utf8');
const imageDraw=rendererSource.slice(rendererSource.indexOf('void Renderer2D::drawImageDirect('),rendererSource.indexOf('void Renderer2D::drawAtlasFrame('));
for(const field of ['x','y','width','height','rotation','opacity']) assert(imageDraw.includes('std::isfinite('+field+')'));
assert(imageDraw.includes('opacity=std::min(opacity,1.0f)'));

assert(main.includes("if(game.setupStarterVisual(selected))"));

const introPresenter=await fs.readFile(new URL('../project/src/runtime/IntroCinematicPresenter.cpp',import.meta.url),'utf8');
const mediaPipeline=await fs.readFile(new URL('../scripts/prepare_native_presentation.py',import.meta.url),'utf8');
assert(introPresenter.includes('Hold the sampled source frame'));
assert(!introPresenter.includes('frameB'));
assert(mediaPipeline.includes('prepare_intro(ROOT)'));
const introPipeline=await fs.readFile(new URL('../scripts/prepare_intro_cinematic.py',import.meta.url),'utf8');
assert(introPipeline.includes('interpolation=cv2.INTER_NEAREST'));
assert(introPresenter.includes('kIntroCinematicPaths[keyframe.page]'));
assert(introPresenter.includes('renderer.retireSpriteSheet(m_sheet)'));

assert(main.includes('starterFormRowAt(touch.px,touch.py)'));
assert(main.includes('kStarterFormBackRect.contains(touch.px,touch.py)'));
assert(setup.includes('renderer.drawWindow(kStarterFormBackRect.x'));
assert(setup.includes('m_prompt.drawCursor(renderer,25,y,labelSize)'));
assert(!setup.includes('renderer.drawText(form ? form->name'));

assert(main.includes('setup.openForms(game)'));
assert(!main.includes('setup.formsOpen=true;setup.selectedForm=0'));
assert(setup.includes('if(formIndexAt(game,ordinal)==current)'));
assert(main.includes('moveStarterFormCursor(setup.selectedForm,count,-int(Pokerogue3DS::kStarterFormPageSize))'));
assert(main.includes('moveStarterFormCursor(setup.selectedForm,count,int(Pokerogue3DS::kStarterFormPageSize))'));

assert(main.includes('if(setupInput && setup.candyStoreOpen)'));
assert(main.includes('setup.handleCandyStoreInput(KEY_A,game,&progress)'));
assert(main.includes('kStarterFormCandyRect.contains(touch.px,touch.py)'));
assert(main.includes('isPaused || partyInput || setupModalInput'));
assert(setup.includes('if(!store) {candyFeedback="Guardado no disponible.";return true;}'));
assert(!setup.includes('auto mutRecord = *rec'));
assert(!setup.includes('applyNativeStarterCostReduction(mutRecord)'));
assert(setup.includes('0x40000000u /* KEY_CPAD_UP'));
assert(setup.includes('0x80000000u /* KEY_CPAD_DOWN'));

assert(setup.includes('if(!candyPriceFor(dex))'));
assert(setup.includes('Captura este Pokémon para usar caramelos.'));
assert(setup.includes('pres==StarterCostPurchaseResult::MissingPrice'));
assert(setup.includes('pres==StarterPassivePurchaseResult::MissingPrice'));

assert(setup.includes("abilityUiName(game.setupStarterPassiveAbilityId(dex))"));

assert(frontend.includes("TouchRect{12,205,296,35}.contains(touchX,touchY)"));
assert(!frontend.includes("No hay partidas finalizadas registradas."));

assert(arena.includes("drawTextFitted(trainerName, 20.0f, 13.0f, 0.45f, 164.0f"));
for(const method of ["drawText", "drawTextWrapped", "drawTextFitted"]) {
 const body=rendererSource.slice(rendererSource.indexOf("Renderer2D::"+method+"("));
 const guard=body.slice(0,body.indexOf("#if"));
 assert(guard.includes("std::isfinite(x)") && guard.includes("std::isfinite(y)"));
}

assert(arena.includes("265.0f, 82.0f, 1.0f, animationTimeMs"));
assert(!arena.includes("265.0f, 82.0f, 1.5f"));
assert(trainerSource.includes("!std::isfinite(anchorX)"));
assert(trainerSource.includes("!std::isfinite(scale) || scale<=0"));

assert(trainerSource.includes("m_metadata.animationFrame(elapsedMs,24,128)"));

assert(!trainerSource.includes("if (m_animationStartMs == 0)"));
assert(trainerSource.includes("m_animationStarted = false"));
assert(trainerSource.includes("if (!m_animationStarted)"));

assert(main.includes("setup.skipIntro(renderer)"));
assert(main.includes("introActive = !setup.introFinished()"));
assert(!main.includes("frameAnimationTimeMs >= (Pokerogue3DS::kIntroTotalDurationMs + 300)"));
assert(introPresenter.includes("if(renderer) renderer->retireSpriteSheet(m_sheet)"));

const introBranch=main.slice(main.indexOf("if (introActive) {\n                renderer.beginFrame()"),main.indexOf("// The menu consumes input"));
assert(introBranch.includes("setup.drawTop") && !introBranch.includes("arena.draw"));

assert(arena.includes("if (!definition) { clear(&renderer); return false; }"));
assert(arena.includes("renderer.retireSpriteSheet(m_layers[i])"));
assert(main.includes("arena.clear(&renderer)"));

assert(main.includes("false,game.experienceLevelCap(),frameAnimationTimeMs"));
const hudSource=await fs.readFile(new URL("../project/include/runtime/BattleHudPresenter.hpp",import.meta.url),"utf8");
assert(hudSource.includes("display->tween.displayedHp(animationTimeMs)"));
assert(hudSource.includes("std::array<HpDisplay,4>"));

assert(main.includes("setup.clear(&renderer)"));
assert(main.includes("battleHud.clear(&renderer)"));
assert(main.includes("setup.releaseIconPages(renderer)"));
assert(setup.includes("m_introCinematic.clear(renderer)"));
assert(hudSource.includes("renderer->retireSpriteSheet(sheet)"));

assert(main.includes("renderer.beginFrame();\n    renderer.endFrame();\n    player.exit()"));

const typeLabelDraw=rendererSource.slice(rendererSource.indexOf("bool Renderer2D::drawTypeLabel("),rendererSource.indexOf("bool Renderer2D::drawHudTypeIcon("));
assert(typeLabelDraw.includes("width<row->frame.sourceWidth || height<row->frame.sourceHeight"));
assert(!typeLabelDraw.includes("float scale="));

assert(main.includes("uiSettings.save(next,preferences.touchControls,preferences.hpBarSpeed,preferences.expGainsSpeed)"));
assert(main.includes("battleHud.setHpBarSpeed(preferences.hpBarSpeed)"));
assert(frontend.includes("m_group==1 && m_selected==2"));
assert(frontend.includes("settings:default\",\"settings:fast\",\"settings:faster\",\"settings:skip"));
const preferencesSource=await fs.readFile(new URL("../project/include/storage/NativePresentationSettings.hpp",import.meta.url),"utf8");
assert(preferencesSource.includes("version>=3 ? unsigned((versionFlags>>17)&3u) : 0u"));
assert(preferencesSource.includes("values[0].hpBarSpeed!=values[1].hpBarSpeed"));

assert(hudSource.includes("m_expTimeline.update(species->growthRate"));
assert(!hudSource.includes("diff / 8"));
assert(hudSource.includes("hudLevelDigitAtlas(player,uint16_t(visibleLevel)"));

assert(main.includes("if(frontend.overlaysTitle()) renderer.drawRect(0,0,400,240,0x60000000)"));

const movePresentation=await fs.readFile(new URL('../project/include/runtime/MoveMenuPresenter.hpp',import.meta.url),'utf8');
assert(movePresentation.includes('game.doubleBattle() ? 196 : 224'));
assert(movePresentation.includes('bounds.width-16'));
assert(movePresentation.includes('const auto back=moveBackRectangle(game.doubleBattle())'));

assert(main.includes("if(struggleActive && touchedMove==0)"));
assert(movePresentation.includes("const auto& bounds=kMoveButtonRects[i]"));
assert(movePresentation.includes("const float nameWidth=bounds.width-28"));
assert(!movePresentation.includes("kMovePos"));

assert(main.includes("moveBackRectangle(game.doubleBattle()).contains(touch.px,touch.py)"));

const partyPresentation=await fs.readFile(new URL('../project/include/runtime/PartyMenuPresenter.hpp',import.meta.url),'utf8');
assert(partyPresentation.includes('y+3,nameSize'));
assert(partyPresentation.includes('hpX + 16, y + 3, 0.24f, 70'));
assert(partyPresentation.includes('std::floor(84*frac)'));
assert(partyPresentation.includes('std::min(actor.battleState.hp,actor.battleState.maxHp)'));
assert(!partyPresentation.includes('hpX + 68'));

assert(partyPresentation.includes("PokemonIconPresenter m_icons{true,6,true}"));
assert(!partyPresentation.includes("y + 2, 1.0f, 0.5f"));

assert(partyPresentation.includes("y + 3, 0.3125f, 112"));
assert(partyPresentation.includes("bounds.x+110,y+17,0.25f"));

assert(partyPresentation.includes("actor.actorIdentityResolved && actor.actor.appearanceResolved && actor.actor.shiny"));
assert(partyPresentation.includes("kStarterVariantIconFrames[variant]"));
assert(main.includes("partyMenu.clear(&renderer)"));

const decisions=await fs.readFile(new URL('../project/include/runtime/DecisionMenuPresenter.hpp',import.meta.url),'utf8');
assert(decisions.includes('bounds=kPartyButtonRects[i]'));
assert(decisions.includes('drawTextFitted(hp,237,y+4,0.3125f,67'));
assert(decisions.includes('PokemonIconPresenter m_icons{true,6,true}'));
assert(!decisions.includes('34,y+5,1,0.5f'));

assert(decisions.includes("textLinesWithinHeight(rect.height-18,renderer.textInkHeight(nameSize),renderer.textLineHeight(nameSize),3)"));

const rewardPresentation=await fs.readFile(new URL('../project/include/runtime/RewardMenuPresenter.hpp',import.meta.url),'utf8');
assert(rewardPresentation.includes('definition ? moveUiName(definition->id)'));
assert(rewardPresentation.includes('27,bounds.y+5,nameSize'));
assert(!rewardPresentation.includes('definition ? definition->name'));

assert(main.includes("if(!partyMenu.open) partyMenu.clear(&renderer)"));
assert(main.includes("if(!game.capturePartyChoicePending()) decisionMenu.releaseIcons(renderer)"));
assert(main.includes("rewardMenu.releasePartyIcons(renderer)"));

assert(main.includes("pauseButtonAt(touch.px,touch.py)"));
assert(main.includes("kPauseButtonRects[i].y+10"));
assert(main.includes("36.0f,y,labelSize"));

assert(main.includes("BattleMenuCommand::ExecuteMove:changed=game.advanceBattleTurn();if(changed) battleMenu.reset()"));

const commandPresentation=await fs.readFile(new URL('../project/include/runtime/BattleCommandMenuPresenter.hpp',import.meta.url),'utf8');
assert(commandPresentation.includes('doubleBattle && (keys & KEY_L)'));
assert(commandPresentation.includes('doubleBattle && (keys & KEY_R)'));
assert(commandPresentation.includes('moveBackRectangle(doubleBattle).contains(x,y)'));
assert(!commandPresentation.includes('y >= 210 && m_page'));

assert(commandPresentation.includes("MoveMenuPresenter::clear(renderer)"));
assert(main.includes("battleMenu.clear(&renderer)"));
assert(main.indexOf("battleMenu.clear(&renderer)") < main.indexOf("renderer.fini()"));

const itemPresentation=await fs.readFile(new URL('../project/include/runtime/ItemIconPresenter.hpp',import.meta.url),'utf8');
assert(itemPresentation.includes('rect, std::round(x), std::round(y), size, size, opacity'));
assert(itemPresentation.indexOf('!std::isfinite(size)') < itemPresentation.indexOf('C2D_SpriteSheetLoad'));
assert(itemPresentation.includes('frame->page >= sizeof(kItemIconPages)/sizeof(kItemIconPages[0])'));
assert(itemPresentation.includes('renderer->retireSpriteSheet(m_sheet)'));
assert(rewardPresentation.includes('m_icons.clear(renderer)'));
assert(main.includes('rewardMenu.clear(&renderer)'));

assert(frontend.includes("pokedexCellAt(touchX,touchY)"));
assert(frontend.includes("const auto bounds=pokedexCellRectangle(cell)"));

assert(frontend.includes("nativeCaughtShinyVariants(*record)"));
assert(frontend.includes("renderer->retireSpriteSheet(m_dexVariants)"));

assert(partyPresentation.includes("m_icons.prepareAppearances(renderer,appearances,std::min(count,6u))"));
assert(partyPresentation.includes("m_icons.drawAppearance(renderer,appearances[i]"));
assert(partyPresentation.includes("resolvePokemonIcon(actor.dex,actor.formId"));

assert(decisions.includes("resolvePokemonIcon(actor.dex,actor.formId"));
assert(decisions.includes("m_icons.drawAppearance(renderer,icons[i].appearance,24,y+1)"));

const iconPresentation=await fs.readFile(new URL("../project/include/runtime/PokemonIconPresenter.hpp",import.meta.url),"utf8");
assert(iconPresentation.includes("image.subtex->width!=expected || image.subtex->height!=(m_nativeTiles ? 32 : expected)"));
assert(iconPresentation.includes("renderer.retireSpriteSheet(selected->sheet);selected->sheet=nullptr"));

const setupPresentation=await fs.readFile(new URL("../project/include/runtime/SetupPresenter.hpp",import.meta.url),"utf8");
assert(setupPresentation.includes("PokemonIconPresenter m_teamIcons{true,6,true}"));
assert(setupPresentation.includes("m_teamIcons.prepareAppearances(renderer,appearances"));
assert(setupPresentation.includes("m_teamIcons.clear(&renderer)"));
assert(setupPresentation.includes("resolvePokemonIcon(actor.dex,actor.formId"));

assert(main.includes("Pokerogue3DS::starterConfirmAt(touch.px,touch.py)"));
assert(!main.includes("touch.py>=116 && touch.py<153"));
assert(!frontend.includes("touchY >= 125 && touchY <= 155"));

assert(frontend.includes("kLoadActionRects[0].contains(touchX,touchY)"));
assert(frontend.includes("kLoadActionRects[1].contains(touchX,touchY)"));
assert(!frontend.includes("touchY >= 115 && touchY <= 145"));

assert(main.includes("battleHud.setExpGainsSpeed(preferences.expGainsSpeed)"));
assert(frontend.includes("settings:expGainsSpeed"));
assert(preferencesSource.includes("version==4 ? unsigned((versionFlags>>19)&3u) : 0u"));

assert(main.includes("game.cycleSetupStarterAbility(1,progress)"));
assert(main.includes("kStarterFormAbilityRect.contains(touch.px,touch.py)"));
assert(setupPresentation.includes("game.canCycleSetupStarterAbility(dex)"));
assert(setupPresentation.includes("abilityUiName(game.setupStarterAbilityId(dex))"));

assert(setupPresentation.includes("kStarterPassiveNameRect,passiveUnlocked ? 0xffffffff : 0xff909090"));
assert(setupPresentation.includes("starter-select-ui-handler:passive"));
assert(setupPresentation.includes("renderer.drawTextFitted(label,161,211,0.3125f,220"));

assert(setupPresentation.includes("const auto iconBounds=starterTeamIconRectangle(i)"));
assert(setupPresentation.includes("teamIcons[i].appearance,iconBounds.x,iconBounds.y"));


assert(partyPresentation.includes('if(!m_icons.draw(renderer,actor.dex,formIndices[i]'));
assert(partyPresentation.includes('if(!m_icons.drawAppearance(renderer,appearances[i]'));

const rewardPartyLayout=await fs.readFile(new URL('../project/include/runtime/RewardMenuPresenter.hpp',import.meta.url),'utf8');
assert(rewardPartyLayout.includes('renderer.drawWindow(kPartyHeaderRect.x'));
assert(rewardPartyLayout.includes('renderer.drawWindow(kPartyFooterRect.x'));
assert(rewardPartyLayout.includes('feedback.empty() ? "Elige el Pokémon destinatario" : feedback.c_str()'));

const pokemonAtlasSource=await fs.readFile(new URL('../project/src/runtime/PokemonAtlasPresenter.cpp',import.meta.url),'utf8');
const spriteBoxBody=pokemonAtlasSource.slice(pokemonAtlasSource.indexOf('void PokemonAtlasPresenter::draw('),pokemonAtlasSource.indexOf('float PokemonAtlasPresenter::calculateProportionalScale'));
assert(spriteBoxBody.indexOf('width<=0 || height<=0')<spriteBoxBody.indexOf('selectMetadata('));
const anchoredBody=pokemonAtlasSource.slice(pokemonAtlasSource.indexOf('void PokemonAtlasPresenter::drawAnchored('),pokemonAtlasSource.indexOf('void PokemonAtlasPresenter::drawTrainerAnchored('));
assert(anchoredBody.indexOf('!std::isfinite(scale) || scale<0')<anchoredBody.indexOf('selectMetadata('));

assert(rewardPartyLayout.includes("renderer.drawTextBox(name,44,bounds.y+5,nameSize,180,nameLines"));
assert(rewardPartyLayout.includes("m_cursor.drawCursor(renderer,27,bounds.y+5,nameSize)"));

assert(decisions.includes("renderer.drawTextBox(actor.localizedName,70,y+4,nameSize,159,nameLines"));
assert(decisions.includes("m_cursor.drawCursor(renderer,10,y+4,nameSize)"));

assert(partyPresentation.includes("bounds.x + 60, y + 17, 0.25f, 42"));

// Mode metadata must stay in the left column, clear of the detail panel at x=151.
assert(setupPresentation.includes('renderer.drawTextFitted(game.presentation().modeName ? game.presentation().modeName : "",16,217,0.375f,124,0xffffffff)'));
assert(!setupPresentation.includes('renderer.drawText(game.presentation().modeName'));

// Battle commands share touch geometry and imported labels, with bounded native text.
for (const key of ['fight','ball','pokemon','run']) {
    assert(commandPresentation.includes('"command-ui-handler:'+key+'"'));
}
assert(commandPresentation.includes('const auto& rect=kCommandButtonRects[i]'));
assert(commandPresentation.includes('runtimeUiText(commandKeys[i])'));
assert(commandPresentation.includes('textWidth=rect.width-38.0f'));
assert(commandPresentation.includes('m_cursor.drawCursor(renderer,textX-10,textY,labelSize)'));
assert(!commandPresentation.includes('} kCmds[]'));

assert(setupPresentation.includes('const char* const confirmationKeys[]={"menu:yes","menu:no"}'));
assert(setupPresentation.includes('const auto& rect=kStarterConfirmRects[i]'));
assert(setupPresentation.includes('rect.x+27,rect.y+7,0.45f,rect.width-35'));
assert(setupPresentation.includes('m_prompt.drawCursor(renderer,rect.x+10,rect.y+7,labelSize)'));
assert(!setupPresentation.includes('drawText(runtimeUiText("menu:yes")'));
assert(setupPresentation.includes('m_prompt.drawCursor(renderer,28,80,costLabelSize)'));
assert(setupPresentation.includes('m_prompt.drawCursor(renderer,28,130,passiveLabelSize)'));

const directImageGuard=rendererSource.slice(rendererSource.indexOf('void Renderer2D::drawImageDirect('),rendererSource.indexOf('void Renderer2D::drawAtlasFrame('));
assert(directImageGuard.includes('img.subtex->left>=img.subtex->right'));
assert(directImageGuard.includes('img.subtex->top<=img.subtex->bottom'));
assert(directImageGuard.includes('if (flipX) scaleX = -scaleX'));
assert(directImageGuard.includes('if (flipY) scaleY = -scaleY'));

const importConfirmation=frontend.slice(frontend.indexOf('const TouchRect buttons[]={kConfirmationYesRect'),frontend.indexOf('} else if(m_page==FrontendPage::History)',frontend.indexOf('const TouchRect buttons[]={kConfirmationYesRect')));
assert(importConfirmation.includes('m_title.drawCursor(renderer,rect.x+7,rect.y+9,labelSize)'));
assert(importConfirmation.includes('runtimeUiText(keys[i])'));
const touchConfirmation=importConfirmation.slice(importConfirmation.indexOf('} else if(m_confirmingTouchDisable)'));
assert(touchConfirmation.includes('renderer.drawWindow(kConfirmationYesRect.x'));
assert(touchConfirmation.includes('renderer.drawWindow(kConfirmationNoRect.x'));
assert(touchConfirmation.includes('kConfirmationYesRect.width-16'));
assert(touchConfirmation.includes('kConfirmationNoRect.width-16'));

const windowDraw=rendererSource.slice(rendererSource.indexOf('bool Renderer2D::drawWindow('),rendererSource.indexOf('bool Renderer2D::drawTypeLabel('));
assert(windowDraw.includes('!std::isfinite(width)'));
assert(windowDraw.includes('width=std::round(width);height=std::round(height)'));
assert(windowDraw.includes('image.subtex->left>=image.subtex->right'));

const utf8Abbreviation=await fs.readFile(new URL('../project/include/runtime/Utf8Abbreviation.hpp',import.meta.url),'utf8');
assert.equal(utf8Abbreviation.split('uint32_t cp=0;if(!utf8CodePoint(out+start,cp) || !jsTrailingSpace(cp)) break;').length-1,2);

assert(rewardPresentation.includes('const auto& claim=kRewardClaimButtonRect'));
assert(rewardPresentation.includes('const auto& skip=kRewardSkipButtonRect'));
assert(rewardPresentation.includes('claim.width-24'));
assert(rewardPresentation.includes('skip.width-24'));
assert(rewardPresentation.includes('cx+10,cardY+142,labelSize'));
assert(rewardPresentation.includes('bounds.x+20,bounds.y+9,labelSize'));
assert(!rewardPresentation.includes('renderer.drawText('));

// Party statuses reuse the imported localized atlas at native scale.
for(const status of ['paralysis','poison','toxic','burn','sleep','freeze']) {
    assert(partyPresentation.includes('statusKey="'+status+'"'));
}
assert(partyPresentation.includes('renderer.drawHudIndicator(statusKey,false,bounds.x+178,y+4)'));
assert(!partyPresentation.includes('renderer.drawText('));
assert(partyPresentation.includes('"PS", hpX, y + 4, 0.20f, 14'));

assert(partyPresentation.includes('renderer.drawHudIndicator("faint",false,bounds.x+178,y+4)'));
