import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
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
