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
assert(arena.includes('m_trainerCurrentFemale != female || m_trainerCurrentName != name'));
assert(arena.includes('m_trainerAttempted=false;m_trainerCurrentFemale=false;m_trainerCurrentName.clear()'));
assert(arena.includes('m_trainer.loadTrainer(trainerTypeId, female,&renderer)'));
assert(!arena.includes('if (!m_trainer.isLoaded() || m_trainerCurrentTypeId'));

const rendererSource=await fs.readFile(new URL('../project/src/gfx/renderer2d.cpp',import.meta.url),'utf8');
const imageDraw=rendererSource.slice(rendererSource.indexOf('void Renderer2D::drawImageDirect('),rendererSource.indexOf('void Renderer2D::drawAtlasFrame('));
for(const field of ['x','y','width','height','rotation','opacity']) assert(imageDraw.includes('std::isfinite('+field+')'));
assert(imageDraw.includes('opacity=std::min(opacity,1.0f)'));
